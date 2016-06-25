#ifndef __ia64
		
		.code	
		
		.proc
		.callinfo
_gp_tas
#ifdef __LP64__
		bve     (rp)
#else
		bv	(rp)
#endif /* __LP64__ */
		ldcws	(arg0), ret0
		.procend

		.export _gp_tas, ENTRY

#else
 /* For ia64 HP-UX. Xindong */
	.radix  C
	.psr    msb
#ifdef __LP64__
	.psr    abi64
#else
	.psr    abi32
#endif

	.text
	.type _gp_tas,@function
	.proc _gp_tas
	_gp_tas::
	mov            ar.ccv = 1
	mov            r29 = 0
#ifndef __LP64__
	addp4      r32 = 0,r32
#endif
	;;
	ld4            r3 = [r32]
	cmpxchg4.acq   r8 = [r32], r29, ar.ccv
	;;
	br.ret.sptk.clr b0
	.endp _gp_tas

#ifdef dummyia64
	.text
	.proc _gp_semclear
_gp_semclear::
	mov            r15=0
	st8.rel        [r32] = r15
	br.ret.sptk.clr b0
	.endp _gp_semclear
#endif /* dummyia64 */
#endif /* __ia64 */

