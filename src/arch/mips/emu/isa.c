/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>

#include "context.h"
#include "isa.h"
#include "regs.h"


/* Debug categories */
int mips_isa_call_debug_category;
int mips_isa_inst_debug_category;

/*
 * Instruction statistics
 */
static long long mips_inst_freq[MIPSInstOpcodeCount];

/* Table including references to functions in machine.c
 * that implement machine instructions. */
/* Instruction execution table */
static mips_isa_inst_func_t mips_isa_inst_func[MIPSInstOpcodeCount] =
{
	NULL /* for op_none */
#define DEFINST(_name, _fmt_str, _op0, _op1, _op2, _op3) , mips_isa_##_name##_impl
#include <arch/mips/asm/Inst.def>
#undef DEFINST
};

/* FIXME - merge with ctx_execute */
void mips_isa_execute_inst(MIPSContext *ctx)
{
	MIPSEmu *emu = ctx->emu;
	MIPSInstOpcode opcode;

	ctx->next_ip = ctx->n_next_ip;
	ctx->n_next_ip += 4;

	/* Debug */
	if (debug_status(mips_isa_inst_debug_category))
	{
		mips_isa_inst_debug("%d %8lld %x: ", ctx->pid,
				asEmu(emu)->instructions, ctx->regs->pc);
		MIPSInstWrapDump(ctx->inst, debug_file(mips_isa_inst_debug_category));
	}

	/* Call instruction emulation function */
	opcode = MIPSInstWrapGetOpcode(ctx->inst);
	if (opcode)
		mips_isa_inst_func[opcode](ctx);
	/* Statistics */
	mips_inst_freq[opcode]++;

	/* Debug */
	mips_isa_inst_debug("\n");
//	if (debug_status(mips_isa_call_debug_category))
//		mips_isa_debug_call(ctx);
}

/* Register File access */
#define MIPS_COP0_GET(X) 		ctx->regs->regs_cop0[X]

#define MIPS_FPR_S_GET(X)		ctx->regs->regs_F.s[X]
#define MIPS_FPR_S_SET(X, V)	ctx->regs->regs_F.s[X] = (V)

#define MIPS_FPR_D_GET(X)		ctx->regs->regs_F.d[X]
#define MIPS_FPR_D_SET(X, V)	ctx->regs->regs_F.d[X] = (V)

#define MIPS_REG_HI 			ctx->regs->regs_HI
#define MIPS_REG_LO 			ctx->regs->regs_LO
#define MIPS_REG_C_FPC_FCSR 	ctx->regs->regs_C.FCSR
#define MIPS_REG_C_FPC_FIR 		ctx->regs->regs_C.FIR

/* Instruction fields */
unsigned int mips_gpr_get_value(MIPSContext* ctx, unsigned int reg_no)
{
	return (ctx->regs->regs_R[reg_no]);
}

void mips_gpr_set_value(MIPSContext *ctx, unsigned int reg_no, unsigned int value)
{
	ctx->regs->regs_R[reg_no] = (value);
}

void mips_isa_branch(MIPSContext *ctx, unsigned int dest)
{
	(ctx->n_next_ip) = dest;
}

void mips_isa_rel_branch(MIPSContext *ctx, unsigned int dest)
{
	ctx->n_next_ip = ctx->regs->pc + (dest) + 4;
}
