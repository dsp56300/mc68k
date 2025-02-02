#pragma once

#include <array>
#include <string>
#include <vector>

#include "gpt.h"
#include "qsm.h"
#include "sim.h"

namespace mc68k
{
	struct CpuState;

	class Mc68k
	{
	public:
		enum class HostEndian
		{
			Big,
			Little
		};

		static constexpr uint32_t CpuStateSize = 600;

		static constexpr HostEndian hostEndian()
		{
			constexpr uint32_t test32 = 0x01020304;
			constexpr uint8_t test8 = static_cast<const uint8_t&>(test32);

			static_assert(test8 == 0x01 || test8 == 0x04, "unable to determine endianess");

			return test8 == 0x01 ? HostEndian::Big : HostEndian::Little;
		}

		Mc68k();
		virtual ~Mc68k();

		virtual uint32_t exec();

		void injectInterrupt(uint8_t _vector, uint8_t _level);
		bool hasPendingInterrupt(uint8_t _vector, uint8_t _level) const;

		virtual void onReset() {}
		virtual uint32_t onIllegalInstruction(uint32_t _opcode);

		static void writeW(uint8_t* _buf, size_t _offset, uint16_t _value)
		{
			auto* p8 = &_buf[_offset];
			auto* p16 = reinterpret_cast<uint16_t*>(p8);

			if constexpr (hostEndian() != HostEndian::Big)
			{
				_value = static_cast<uint16_t>((_value << 8) | (_value >> 8));
			}

			*p16 = _value;
		}

		static void writeW(std::vector<uint8_t>& _buf, size_t _offset, uint16_t _value)
		{
			writeW(_buf.data(), _offset, _value);
		}

		static uint16_t readW(const uint8_t* _buf, size_t _offset)
		{
			const auto* ptr = &_buf[_offset];

			const auto v16 = *reinterpret_cast<const uint16_t*>(ptr);

			if constexpr (hostEndian() == HostEndian::Big)
			{
				return v16;
			}

			return static_cast<uint16_t>(v16 << 8 | v16 >> 8);
		}

		static uint16_t readW(const std::vector<uint8_t>& _buf, size_t _offset)
		{
			return readW(_buf.data(), _offset);
		}

		virtual uint8_t read8(uint32_t _addr);
		virtual uint16_t read16(uint32_t _addr);

		uint32_t read32(uint32_t _addr)
		{
			uint32_t res = static_cast<uint32_t>(read16(_addr)) << 16;
			res |= read16(_addr + 2);
			return res;
		}

		virtual void write8(uint32_t _addr, uint8_t _val);
		virtual void write16(uint32_t _addr, uint16_t _val);

		void write32(uint32_t _addr, uint32_t _val)
		{
			write16(_addr, _val >> 16);
			write16(_addr + 2, _val & 0xffff);
		}


		virtual uint16_t readImm16(uint32_t _addr) = 0;

		uint32_t readImm32(uint32_t _addr)
		{
			uint32_t res = static_cast<uint32_t>(readImm16(_addr)) << 16;
			res |= readImm16(_addr + 2);
			return res;
		}

		uint32_t readIrqUserVector(uint8_t _level);

		void reset();
		void setPC(uint32_t _pc);
		uint32_t getPC() const;
		virtual uint32_t getResetPC() { return 0; }
		virtual uint32_t getResetSP() { return 0; }

		uint32_t disassemble(uint32_t _pc, char* _buffer);

		uint64_t getCycles() const { return m_cycles; }
		
		Port& getPortE()	{ return m_sim.getPortE(); }
		Port& getPortF()	{ return m_sim.getPortF(); }
		Port& getPortGP()	{ return m_gpt.getPortGP(); }
		Port& getPortQS()	{ return m_qsm.getPortQS(); }

		Gpt& getGPT()		{ return m_gpt; }
		Qsm& getQSM()		{ return m_qsm; }
		Sim& getSim()		{ return m_sim; }

		CpuState* getCpuState();
		const CpuState* getCpuState() const;

		bool dumpAssembly(const std::string& _filename, uint32_t _first, uint32_t _count, bool _splitFunctions = true);
		
	private:
		void raiseIPL();

		std::array<uint8_t, CpuStateSize> m_cpuStateBuf;
		CpuState* m_cpuState;

		Gpt m_gpt;
		Sim m_sim;
		Qsm m_qsm;
		
		std::array<std::deque<uint8_t>, 8> m_pendingInterrupts;

		uint64_t m_cycles = 0;
	};
}
