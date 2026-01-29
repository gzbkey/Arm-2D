	.section	__TEXT,__text,regular,pure_instructions
	.build_version macos, 26, 0	sdk_version 26, 2
	.globl	_uaddlb_array                   ; -- Begin function uaddlb_array
	.p2align	2
_uaddlb_array:                          ; @uaddlb_array
	.cfi_startproc
; %bb.0:
	umullb	z0.h, z0.b, z1.b
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	_uadd                           ; -- Begin function uadd
	.p2align	2
_uadd:                                  ; @uadd
	.cfi_startproc
; %bb.0:
	add	z0.s, z1.s, z0.s
	ret
	.cfi_endproc
                                        ; -- End function
	.globl	___arm_2d_sve2_rgb565_fill_colour_with_src_mask ; -- Begin function __arm_2d_sve2_rgb565_fill_colour_with_src_mask
	.p2align	2
___arm_2d_sve2_rgb565_fill_colour_with_src_mask: ; @__arm_2d_sve2_rgb565_fill_colour_with_src_mask
	.cfi_startproc
; %bb.0:
	cbz	x2, LBB2_3
; %bb.1:
	mov	x8, #0                          ; =0x0
LBB2_2:                                 ; =>This Inner Loop Header: Depth=1
	whilelo	p0.h, x8, x2
	ld1b	{ z0.h }, p0/z, [x0, x8]
	ld1h	{ z1.h }, p0/z, [x1, x8, lsl #1]
	add	z0.h, z1.h, z0.h
	st1h	{ z0.h }, p0, [x1, x8, lsl #1]
	add	x8, x8, #32
	cmp	x8, x2
	b.lo	LBB2_2
LBB2_3:
	ret
	.cfi_endproc
                                        ; -- End function
.subsections_via_symbols
