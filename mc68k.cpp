#include "mc68k.h"

#include <cassert>
#include <atomic>
#include <fstream>
#include <cstring>	// strstr

#include "logging.h"

#include "Musashi/m68kcpu.h"

struct CpuState : m68ki_cpu_core
{
	mc68k::Mc68k* instance = nullptr;
};

std::atomic<mc68k::Mc68k*> g_instance = nullptr;

mc68k::Mc68k* getInstance(m68ki_cpu_core* _core)
{
	return static_cast<CpuState*>(_core)->instance;
}

extern "C"
{
	unsigned int m68k_read_immediate_16(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->readImm16(address);
	}
	unsigned int m68k_read_immediate_32(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->readImm32(address);
	}

	unsigned int m68k_read_pcrelative_8(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read8(address);
	}
	unsigned int m68k_read_pcrelative_16(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read16(address);
	}
	unsigned int m68k_read_pcrelative_32(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read32(address);
	}

	unsigned int m68k_read_memory_8(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read8(address);
	}
	unsigned int m68k_read_memory_16(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read16(address);
	}
	unsigned int m68k_read_memory_32(m68ki_cpu_core* core, unsigned int address)
	{
		return getInstance(core)->read32(address);
	}
	void m68k_write_memory_8(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		getInstance(core)->write8(address, static_cast<uint8_t>(value));
	}
	void m68k_write_memory_16(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		getInstance(core)->write16(address, static_cast<uint16_t>(value));
	}
	void m68k_write_memory_32(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		getInstance(core)->write32(address, value);
	}
	int m68k_int_ack(m68ki_cpu_core* core, int int_level)
	{
		return static_cast<int>(getInstance(core)->readIrqUserVector(static_cast<uint8_t>(int_level)));
	}
	int read_sp_on_reset(m68ki_cpu_core* core)
	{
		return static_cast<int>(getInstance(core)->getResetSP());
	}
	int read_pc_on_reset(m68ki_cpu_core* core)
	{
		return static_cast<int>(getInstance(core)->getResetPC());
	}
	int m68k_illegal_cbk(m68ki_cpu_core* core, int opcode)
	{
		return static_cast<int>(getInstance(core)->onIllegalInstruction(static_cast<uint32_t>(opcode)));
	}
	void m68k_reset_cbk(m68ki_cpu_core* core)
	{
		return getInstance(core)->onReset();
	}

	unsigned int m68k_read_disassembler_8  (unsigned int address)
	{
		mc68k::Mc68k* instance = g_instance;
		return m68k_read_memory_8(instance->getCpuState(), address);
	}
	unsigned int m68k_read_disassembler_16 (unsigned int address)
	{
		mc68k::Mc68k* instance = g_instance;
		return m68k_read_memory_16(instance->getCpuState(), address);
	}
	unsigned int m68k_read_disassembler_32 (unsigned int address)
	{
		mc68k::Mc68k* instance = g_instance;
		return m68k_read_memory_32(instance->getCpuState(), address);
	}
}

namespace mc68k
{
	Mc68k::Mc68k() : m_gpt(*this), m_sim(*this), m_qsm(*this)
	{
		m_cpuStateBuf.fill(0);

		static_assert(sizeof(CpuState) <= CpuStateSize);
		m_cpuState = reinterpret_cast<CpuState*>(&m_cpuStateBuf[0]);

		g_instance = this;

		getCpuState()->instance = this;

		m68k_set_cpu_type(getCpuState(), M68K_CPU_TYPE_68020);
		m68k_init(getCpuState());
		m68k_set_int_ack_callback(getCpuState(), m68k_int_ack);
		m68k_set_illg_instr_callback(getCpuState(), m68k_illegal_cbk);
		m68k_set_reset_instr_callback(getCpuState(), m68k_reset_cbk);
	}
	Mc68k::~Mc68k()
	{
		auto* inst = this;
		g_instance.compare_exchange_strong(inst, nullptr);
	};

	uint32_t Mc68k::exec()
	{
		const auto deltaCycles = m68k_execute(getCpuState(), 1);
		m_cycles += deltaCycles;

		m_gpt.exec(deltaCycles);
		m_sim.exec(deltaCycles);
		m_qsm.exec(deltaCycles);

		return deltaCycles;
	}

	void Mc68k::injectInterrupt(uint8_t _vector, uint8_t _level)
	{
		m_pendingInterrupts[_level].push_back(_vector);
		raiseIPL();
	}

	bool Mc68k::hasPendingInterrupt(uint8_t _vector, uint8_t _level) const
	{
		const auto& ints = m_pendingInterrupts[_level];
		for (const uint8 i : ints)
		{
			if(i == _vector)
				return true;
		}
		return false;
	}

	void Mc68k::writeW(uint8_t* _buf, const size_t _offset, uint16_t _value)
	{
		auto* p8 = &_buf[_offset];
		auto* p16 = reinterpret_cast<uint16_t*>(p8);

		if constexpr (hostEndian() != HostEndian::Big)
		{
			_value = static_cast<uint16_t>((_value << 8) | (_value >> 8));
		}

		*p16 = _value;
	}

	uint16_t Mc68k::readW(const uint8_t* _buf, const size_t _offset)
	{
		const auto* ptr = &_buf[_offset];

		const auto v16 = *reinterpret_cast<const uint16_t*>(ptr);

		if constexpr (hostEndian() == HostEndian::Big)
		{
			return v16;
		}

		return static_cast<uint16_t>(v16 << 8 | v16 >> 8);
	}

	uint8_t Mc68k::read8(uint32_t _addr)
	{
		const auto addr = static_cast<PeriphAddress>(_addr & g_peripheralMask);

		if(m_gpt.isInRange(addr))			return m_gpt.read8(addr);
		if(m_sim.isInRange(addr))			return m_sim.read8(addr);
		if(m_qsm.isInRange(addr))			return m_qsm.read8(addr);

		return 0;
	}

	uint16_t Mc68k::read16(uint32_t _addr)
	{
		const auto addr = static_cast<PeriphAddress>(_addr & g_peripheralMask);

		if(m_gpt.isInRange(addr))			return m_gpt.read16(addr);
		if(m_sim.isInRange(addr))			return m_sim.read16(addr);
		if(m_qsm.isInRange(addr))			return m_qsm.read16(addr);

		return 0;
	}

	uint32_t Mc68k::read32(const uint32_t _addr)
	{
		uint32_t res = static_cast<uint32_t>(read16(_addr)) << 16;
		res |= read16(_addr + 2);
		return res;
	}

	void Mc68k::write8(uint32_t _addr, uint8_t _val)
	{
		const auto addr = static_cast<PeriphAddress>(_addr & g_peripheralMask);

		if(m_gpt.isInRange(addr))			m_gpt.write8(addr, _val);
		else if(m_sim.isInRange(addr))		m_sim.write8(addr, _val);
		else if(m_qsm.isInRange(addr))		m_qsm.write8(addr, _val);
	}

	void Mc68k::write16(uint32_t _addr, uint16_t _val)
	{
		const auto addr = static_cast<PeriphAddress>(_addr & g_peripheralMask);

		if(m_gpt.isInRange(addr))			m_gpt.write16(addr, _val);
		else if(m_sim.isInRange(addr))		m_sim.write16(addr, _val);
		else if(m_qsm.isInRange(addr))		m_qsm.write16(addr, _val);
	}

	void Mc68k::write32(uint32_t _addr, uint32_t _val)
	{
		write16(_addr, _val >> 16);
		write16(_addr + 2, _val & 0xffff);
	}

	uint32_t Mc68k::readImm32(uint32_t _addr)
	{
		uint32_t res = static_cast<uint32_t>(readImm16(_addr)) << 16;
		res |= readImm16(_addr + 2);
		return res;
	}

	uint32_t Mc68k::readIrqUserVector(const uint8_t _level)
	{
		auto& vecs = m_pendingInterrupts[_level];

		if(vecs.empty())
			return M68K_INT_ACK_AUTOVECTOR;

		const auto vec = vecs.front();
		vecs.pop_front();

		m68k_set_irq(getCpuState(), 0);
		this->raiseIPL();

		return vec;
	}

	void Mc68k::reset()
	{
		m68k_pulse_reset(getCpuState());
	}

	void Mc68k::setPC(uint32_t _pc)
	{
		m68k_set_reg(getCpuState(), M68K_REG_PC, _pc);
	}

	uint32_t Mc68k::getPC() const
	{
		return m68k_get_reg(getCpuState(), M68K_REG_PC);
	}

	uint32_t Mc68k::disassemble(uint32_t _pc, char* _buffer)
	{
		return m68k_disassemble(_buffer, _pc, m68k_get_reg(getCpuState(), M68K_REG_CPU_TYPE));
	}

	CpuState* Mc68k::getCpuState()
	{
		return m_cpuState;
	}

	const CpuState* Mc68k::getCpuState() const
	{
		return m_cpuState;
	}

	bool Mc68k::dumpAssembly(const std::string& _filename, uint32_t _first, uint32_t _count, bool _splitFunctions/* = true*/)
	{
		std::ofstream f(_filename, std::ios::out);

		if(!f.is_open())
			return false;

		for(uint32_t i=_first; i<_first + _count;)
		{
			char disasm[64];
			const auto opSize = disassemble(i, disasm);
			f << MCHEXN(i,6) << ": " << disasm << '\n';
			if(!opSize)
				++i;
			else
				i += opSize;

			auto startsWith = [&](const char* _search)
			{
				return strstr(disasm, _search) == disasm;
			};

			if(startsWith("rts") || startsWith("bra ") || startsWith("jmp "))
				f << '\n';
		}
		f.close();
		return true;
	}

	void Mc68k::raiseIPL()
	{
		for(int i=static_cast<int>(m_pendingInterrupts.size())-1; i>0; --i)
		{
			if(!m_pendingInterrupts[i].empty())
			{
				m68k_set_irq(getCpuState(), static_cast<uint8_t>(i));
				break;
			}
		}
	}
}
