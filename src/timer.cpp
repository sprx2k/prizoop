
#include "platform.h"
#include "cpu.h"
#include "interrupts.h"

// based on timer div freq
int bitsToShift[4] = {
	10,
	4,
	6,
	8
};

void updateDiv() {
	cpu.div += (cpu.clocks - cpu.divBase);
	cpu.divBase = cpu.clocks;
}

void updateTimer() {
	if (cpu.memory.TAC_timerctl & 0x04) {
		const unsigned int bits = bitsToShift[cpu.memory.TAC_timerctl & 3];
		const unsigned int subMask = (1U << bits) - 1U;
		unsigned int elapsed = cpu.clocks - cpu.timerBase;
		unsigned int total = cpu.timer + elapsed;
		unsigned int count = total >> bits;
		unsigned int subTicks = total & subMask;

		if (count >= 256U) {
			// overflow occured
			cpu.memory.IF_intflag |= INTERRUPTS_TIMER;
			unsigned int reload = cpu.memory.TMA_timermodulo;
			unsigned int period = 256U - reload;
			count = reload + ((count - 256U) % period);
		}
		cpu.timer = (count << bits) | subTicks;
		cpu.memory.TIMA_timerctr = (unsigned char)count;

		// determine next interrupt hit for optimizations
		unsigned int ticksToOverflow = ((256U - count) << bits) - subTicks;
		cpu.timerInterrupt = cpu.clocks + ticksToOverflow;
	}
	else {
		cpu.timerInterrupt = 0xFFFFFFFF;
	}
	cpu.timerBase = cpu.clocks;
}

void writeTIMA(unsigned char value) {
	updateTimer();
	const unsigned int bits = bitsToShift[cpu.memory.TAC_timerctl & 3];
	const unsigned int subMask = (1U << bits) - 1U;
	unsigned int subTicks = cpu.timer & subMask;
	cpu.memory.TIMA_timerctr = value;
	cpu.timer = ((unsigned int)value << bits) | subTicks;
	cpu.timerBase = cpu.clocks;
	updateTimer();
}

void writeTAC(unsigned char value) {
	const unsigned int oldBits = bitsToShift[cpu.memory.TAC_timerctl & 3];
	const unsigned int oldSubMask = (1U << oldBits) - 1U;
	if (cpu.memory.TAC_timerctl & 0x04) {
		updateTimer();
	} else {
		cpu.timerBase = cpu.clocks;
	}
	unsigned int oldSubTicks = cpu.timer & oldSubMask;

	cpu.memory.TAC_timerctl = value & 0x07;
	const unsigned int newBits = bitsToShift[cpu.memory.TAC_timerctl & 3];
	const unsigned int newSubMask = (1U << newBits) - 1U;
	cpu.timer = ((unsigned int)cpu.memory.TIMA_timerctr << newBits) | (oldSubTicks & newSubMask);
	cpu.timerBase = cpu.clocks;
	updateTimer();
}

void resetTimerSubTicks() {
	updateTimer();
	const unsigned int bits = bitsToShift[cpu.memory.TAC_timerctl & 3];
	cpu.timer = ((unsigned int)cpu.memory.TIMA_timerctr << bits);
	cpu.timerBase = cpu.clocks;
	updateTimer();
}