#pragma once

#include "cpuState.h"

#ifndef MC68K_CLASS
#define MC68K_CLASS mc68k::Mc68k
#endif

MC68K_CLASS* mc68k_get_instance(m68ki_cpu_core* _core)
{
	return static_cast<MC68K_CLASS*>(static_cast<mc68k::CpuState*>(_core)->instance);
}

extern "C"
{
	unsigned int m68k_read_immediate_16(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->readImm16(address);
	}
	unsigned int m68k_read_immediate_32(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->readImm32(address);
	}

	unsigned int m68k_read_pcrelative_8(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read8(address);
	}
	unsigned int m68k_read_pcrelative_16(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read16(address);
	}
	unsigned int m68k_read_pcrelative_32(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read32(address);
	}

	unsigned int m68k_read_memory_8(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read8(address);
	}
	unsigned int m68k_read_memory_16(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read16(address);
	}
	unsigned int m68k_read_memory_32(m68ki_cpu_core* core, unsigned int address)
	{
		return mc68k_get_instance(core)->read32(address);
	}
	void m68k_write_memory_8(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		mc68k_get_instance(core)->write8(address, static_cast<uint8_t>(value));
	}
	void m68k_write_memory_16(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		mc68k_get_instance(core)->write16(address, static_cast<uint16_t>(value));
	}
	void m68k_write_memory_32(m68ki_cpu_core* core, unsigned int address, unsigned int value)
	{
		mc68k_get_instance(core)->write32(address, value);
	}
	int read_sp_on_reset(m68ki_cpu_core* core)
	{
		return static_cast<int>(mc68k_get_instance(core)->getResetSP());
	}
	int read_pc_on_reset(m68ki_cpu_core* core)
	{
		return static_cast<int>(mc68k_get_instance(core)->getResetPC());
	}
}
