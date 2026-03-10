#ifndef HMI_PARSER_ZEPHYR_HPP
#define HMI_PARSER_ZEPHYR_HPP

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#include "struct_fields.hpp"

static_assert(CHAR_BIT == 8, "HmiParser requires 8-bit bytes");
static_assert(std::is_same_v<uint8_t, unsigned char>,
              "HmiParser expects uint8_t == unsigned char");
static_assert(sizeof(uint16_t) == 2, "HmiParser expects 16-bit uint16_t");
static_assert(sizeof(uint32_t) == 4, "HmiParser expects 32-bit uint32_t");

struct MasterMeasurements {
  float ai1;
  float supercapacitorCurrent;
  float ai3;
  float ai4;
  float ai5;
  float accelPedalVoltage;
  float brakePedalVoltage;
  float accessoryBatteryCurrent;
  float hydrogenHighPressure;
  float hydrogenLeakageSensorVoltage;
  float fuelCellOutputCurrent;
  float motorControllerSupplyCurrent;
  float accessoryBatteryVoltage;
  float fuelCellOutputVoltage;
  float supercapacitorVoltage;
  float motorControllerSupplyVoltage;
  uint8_t din;
  float rpm;
  float speed;
};

struct MasterStatus {
  enum State : uint8_t { Idle, Running, Shutdown, Failure };

  uint8_t serialNumber = 1;
  uint8_t state = Idle;
  bool mainValveEnableOutput = false;
  bool motorControllerEnableOutput = false;
  float accelOutputVoltage = 0.0f;
  float brakeOutputVoltage = 0.0f;
};

struct ProtiumValues {
  float FC_V;
  float FC_A;
  float FC_W;
  float Energy;
  float FCT1;
  float FAN;
  float H2P1;
  float H2P2;
  float TankP;
  float TankT;
  float V_Set;
  float I_Set;
  float UCB_V;
  float Stasis_selector;
  float STASIS_V1;
  float STASIS_V2;
  float Number_of_cell;
  float FCT2;
  float BLW;
  float BattV;
  float IP;
  float TP;
};

enum ProtiumOperatingState : uint8_t {
  Disconnected,
  SystemOff,
  FirmwareVersion,
  CommandNotFound,
  EnteringToStartingPhase,
  EnteringToRunningPhase,
  AnodeSupplyPressureCheck,
  TemperatureCheck,
  FCGasSystemCheck,
  FCSealingCheck,
  FCVoltageCheck,
  LowH2Supply,
  ShutdownInitiated,
  AbnormalShutdownInitiated,
  Running
};

struct ProtiumOperatingStateLogEntry {
  uint32_t msClockTickCount = 0;
  uint8_t state = Disconnected;
};

class HmiParser {
public:
  using MasterMeasurementsCallback = std::function<void(
      uint32_t msClockTickCount, uint32_t cycleClockTickCount,
      const MasterMeasurements &measurements)>;

  using MasterStatusCallback = std::function<void(uint32_t msClockTickCount,
                                                  uint32_t cycleClockTickCount,
                                                  const MasterStatus &status)>;

  using ProtiumValuesCallback = std::function<void(
      uint32_t msClockTickCount, uint32_t cycleClockTickCount,
      const ProtiumValues &values)>;

  using ProtiumOperatingStateCallback = std::function<void(
      uint32_t msClockTickCount, uint32_t cycleClockTickCount,
      ProtiumOperatingState currentOperatingState,
      const ProtiumOperatingStateLogEntry (&operatingStateLogEntries)[8])>;

  MasterMeasurementsCallback onMasterMeasurements;
  MasterStatusCallback onMasterStatus;
  ProtiumValuesCallback onProtiumValues;
  ProtiumOperatingStateCallback onProtiumOperatingState;

  bool processOctet(uint8_t octet) {
    if (_state != State::Start) {
      if (_esc) {
        _esc = false;

        if (octet == kSpecialCharSub) {
          octet = kSpecialChar;
        } else if (octet == kEscCharSub) {
          octet = kEscChar;
        } else {
          ++_badMsgCount;
          printf("HMI: Invalid escape sequence: 0x%02x\n",
                             static_cast<unsigned>(octet));
          resetFrameReception();
          return false;
        }
      } else if (octet == kSpecialChar) {
        startNewFrame();
        return false;
      } else if (octet == kEscChar) {
        _esc = true;
        return false;
      }
    } else {
      _esc = false;
    }

    switch (_state) {
    case State::Start:
      if (octet == kSpecialChar) {
        startNewFrame();
      }
      break;

    case State::Length:
      _crc.processByte(octet);
      _msgLength = octet;
      _writeIndex = 0;
      _state = (_msgLength == 0U) ? State::Crc1 : State::Data;
      break;

    case State::Data:
      _crc.processByte(octet);

      if (_writeIndex < kDataSize) {
        _buffer[_writeIndex] = octet;
      }

      ++_writeIndex;
      if (_writeIndex >= _msgLength) {
        _state = State::Crc1;
      }
      break;

    case State::Crc1:
      _rcvdCrc = static_cast<uint32_t>(octet);
      _state = State::Crc2;
      break;

    case State::Crc2:
      _rcvdCrc |= static_cast<uint32_t>(octet) << 8;
      _state = State::Crc3;
      break;

    case State::Crc3:
      _rcvdCrc |= static_cast<uint32_t>(octet) << 16;
      _state = State::Crc4;
      break;

    case State::Crc4: {
      _rcvdCrc |= static_cast<uint32_t>(octet) << 24;

      const uint32_t calculatedCrc = _crc.checksum();
      resetFrameReception();

      if (_rcvdCrc != calculatedCrc) {
        ++_badMsgCount;
        printf("HMI: Invalid crc - expected %u, received %u\n",
                           static_cast<unsigned>(calculatedCrc),
                           static_cast<unsigned>(_rcvdCrc));
        return false;
      }

      ++_goodMsgCount;
      return processMessage();
    }
    }

    return false;
  }

  std::size_t goodMsgCount() const { return _goodMsgCount; }
  std::size_t badMsgCount() const { return _badMsgCount; }

  void resetCounters() {
    _goodMsgCount = 0;
    _badMsgCount = 0;
  }

private:
  class Crc32 {
  public:
    void reset() { _crc = 0xFFFFFFFFu; }

    void processByte(uint8_t byte) {
      _crc ^= static_cast<uint32_t>(byte);
      for (int bit = 0; bit < 8; ++bit) {
        const bool lsbSet = (_crc & 1u) != 0u;
        _crc >>= 1;
        if (lsbSet) {
          _crc ^= 0xEDB88320u;
        }
      }
    }

    uint32_t checksum() const { return _crc ^ 0xFFFFFFFFu; }

  private:
    uint32_t _crc = 0xFFFFFFFFu;
  };

  template <typename T>
  static constexpr auto getSerializedSize()
      -> std::enable_if_t<!std::is_class_v<T> && !std::is_array_v<T>,
                          std::size_t> {
    return sizeof(T);
  }

  template <typename T>
  static constexpr auto getSerializedSize()
      -> std::enable_if_t<std::is_class_v<T> && !std::is_array_v<T>,
                          std::size_t> {
    return sizeOfFields<T>();
  }

  template <typename T>
  static constexpr auto getSerializedSize()
      -> std::enable_if_t<std::is_array_v<T>, std::size_t> {
    using Element = std::remove_reference_t<decltype(std::declval<T &>()[0])>;
    return getSerializedSize<Element>() * std::extent_v<T>;
  }

  template <typename T>
  auto retrieve(bool &success, std::size_t &readIndex, T &value)
      -> std::enable_if_t<!std::is_class_v<T> && !std::is_array_v<T>> {
    if (!success) {
      return;
    }

    if ((readIndex + sizeof(T)) > _msgLength) {
      success = false;
      return;
    }

    std::memcpy(&value, _buffer + readIndex, sizeof(T));
    readIndex += sizeof(T);
  }

  template <typename T>
  auto retrieve(bool &success, std::size_t &readIndex, T &value)
      -> std::enable_if_t<std::is_class_v<T> && !std::is_array_v<T>> {
    applyToFields(value,
                  [&](auto &field) { retrieve(success, readIndex, field); });
  }

  template <typename T>
  auto retrieve(bool &success, std::size_t &readIndex, T &value)
      -> std::enable_if_t<std::is_array_v<T>> {
    for (auto &element : value) {
      retrieve(success, readIndex, element);
    }
  }

  template <typename... Args> bool receive(Args &...args) {
    constexpr std::size_t payloadLength =
        (getSerializedSize<Args>() + ... + 0U);
    static_assert(payloadLength <= 0xFFu,
                  "Serialized payload is larger than protocol length field");

    if ((_writeIndex < payloadLength) || (_msgLength < payloadLength) ||
        (payloadLength > kDataSize)) {
      return false;
    }

    std::size_t readIndex = 0;
    bool success = true;
    (retrieve(success, readIndex, args), ...);
    return success && (readIndex == payloadLength);
  }

  bool processMasterMeasurementsMsg() {
    MasterMeasurements measurements{};
    uint32_t messageId = 0;
    uint32_t msClockTickCount = 0;
    uint32_t cycleClockTickCount = 0;

    if (!receive(messageId, msClockTickCount, cycleClockTickCount,
                 measurements)) {
      printf("HMI: Could not parse master measurements message\n");
      return false;
    }

    if (onMasterMeasurements) {
      onMasterMeasurements(msClockTickCount, cycleClockTickCount, measurements);
    }

    return true;
  }

  bool processMasterStatusMsg() {
    MasterStatus status{};
    uint32_t messageId = 0;
    uint32_t msClockTickCount = 0;
    uint32_t cycleClockTickCount = 0;

    if (!receive(messageId, msClockTickCount, cycleClockTickCount, status)) {
      printf("HMI: Could not parse master status message\n");
      return false;
    }

    if (onMasterStatus) {
      onMasterStatus(msClockTickCount, cycleClockTickCount, status);
    }

    return true;
  }

  bool processProtiumValuesMsg() {
    ProtiumValues values{};
    uint32_t messageId = 0;
    uint32_t msClockTickCount = 0;
    uint32_t cycleClockTickCount = 0;

    if (!receive(messageId, msClockTickCount, cycleClockTickCount, values)) {
      printf("HMI: Could not parse protium values message\n");
      return false;
    }

    if (onProtiumValues) {
      onProtiumValues(msClockTickCount, cycleClockTickCount, values);
    }

    return true;
  }

  bool processProtiumOperatingStateMsg() {
    ProtiumOperatingStateLogEntry log[8]{};
    ProtiumOperatingState currentOperatingState = Disconnected;
    uint32_t messageId = 0;
    uint32_t msClockTickCount = 0;
    uint32_t cycleClockTickCount = 0;

    if (!receive(messageId, msClockTickCount, cycleClockTickCount,
                 currentOperatingState, log)) {
      printf(
          "HMI: Could not parse protium operating state message\n");
      return false;
    }

    if (onProtiumOperatingState) {
      onProtiumOperatingState(msClockTickCount, cycleClockTickCount,
                              currentOperatingState, log);
    }

    return true;
  }

  bool processMessage() {
    uint32_t messageId = 0;

    if (!receive(messageId)) {
      printf("HMI: Message with no ID\n");
      return false;
    }

    switch (static_cast<HmiMessageId>(messageId)) {
    case HmiMessageId::MasterMeasurements:
      return processMasterMeasurementsMsg();
    case HmiMessageId::MasterStatus:
      return processMasterStatusMsg();
    case HmiMessageId::ProtiumValues:
      return processProtiumValuesMsg();
    case HmiMessageId::ProtiumOperatingState:
      return processProtiumOperatingStateMsg();
    default:
      printf("HMI: Message with unknown ID: %u\n",
                         static_cast<unsigned>(messageId));
      return false;
    }
  }

  void startNewFrame() {
    _state = State::Length;
    _esc = false;
    _rcvdCrc = 0;
    _msgLength = 0;
    _writeIndex = 0;
    _crc.reset();
    std::memset(_buffer, 0, sizeof(_buffer));
  }

  void resetFrameReception() {
    _state = State::Start;
    _esc = false;
    _rcvdCrc = 0;
    _msgLength = 0;
    _writeIndex = 0;
  }

  enum struct HmiMessageId : uint32_t {
    MasterMeasurements = 0xE7696EFE,
    MasterStatus = 0x5D79ED90,
    ProtiumValues = 0xB1236E43,
    ProtiumOperatingState = 0x093C3952
  };

  enum class State : uint8_t { Start, Length, Data, Crc1, Crc2, Crc3, Crc4 };

  static constexpr uint8_t kSpecialChar = 0xFE;
  static constexpr uint8_t kEscChar = 0xDE;
  static constexpr uint8_t kSpecialCharSub = 0xFC;
  static constexpr uint8_t kEscCharSub = 0xDC;
  static constexpr std::size_t kDataSize = 1024;

  Crc32 _crc;
  uint32_t _rcvdCrc = 0;
  State _state = State::Start;
  bool _esc = false;
  std::size_t _msgLength = 0;
  uint8_t _buffer[kDataSize]{};
  std::size_t _writeIndex = 0;
  std::size_t _goodMsgCount = 0;
  std::size_t _badMsgCount = 0;
};

#endif
