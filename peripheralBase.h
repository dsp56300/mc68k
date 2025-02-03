#pragma once

#include <cstdint>
#include <array>

#include "peripheralTypes.h"
#include "memoryOps.h"

namespace mc68k
{
	inline void periphBaseWriteW(uint8_t* _buffer, uint32_t _addr, uint16_t _val)
	{
		memoryOps::writeU16(_buffer, _addr, _val);
	}
	inline uint16_t periphBaseReadW(const uint8_t* _buffer, uint32_t _addr)
	{
		return memoryOps::readU16(_buffer, _addr);
	}

	template<uint32_t Base, uint32_t Size>
	class PeripheralBase
	{
	public:
		explicit PeripheralBase() : m_buffer{0} {}
		virtual ~PeripheralBase() = default;

		constexpr bool isInRange(PeriphAddress _addr) const
		{
			return static_cast<uint32_t>(_addr) >= Base && static_cast<uint32_t>(_addr) < (Base + m_buffer.size());
		}
		virtual uint8_t read8(PeriphAddress _addr)
		{
			return m_buffer[static_cast<uint32_t>(_addr) - Base];
		}
		virtual uint16_t read16(PeriphAddress _addr)
		{
			return periphBaseReadW(m_buffer.data(), static_cast<uint32_t>(_addr) - Base);
		}
		virtual void write8(PeriphAddress _addr, uint8_t _val)
		{
			m_buffer[static_cast<uint32_t>(_addr) - Base] = _val;
		}
		virtual void write16(PeriphAddress _addr, const uint16_t _val)
		{
			return periphBaseWriteW(m_buffer.data(), static_cast<uint32_t>(_addr) - Base, _val);
		}

		virtual void exec(uint32_t _deltaCycles) {}

		static constexpr uint32_t base() { return Base; }
		static constexpr uint32_t size() { return Size; }

	private:
		std::array<uint8_t, Size> m_buffer;
	};
}
