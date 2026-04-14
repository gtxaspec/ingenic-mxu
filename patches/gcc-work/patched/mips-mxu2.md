;; Machine Description for MIPS INGENIC MXU2
;; Based on the Ingenic MXU2 XBurst@Instruction Set Arch Release Date: Nov. 18,2014
;; Contributed by Qian Liu (qian.liu@ingenic.com), Ingenic.
;;
;; Copyright (C) 2014 Free Software Foundation, Inc.
;; Contributed by Qian Liu, qian.liu@ingenic.com.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 3, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING3.  If not see
;; <http://www.gnu.org/licenses/>.
;;

(define_c_enum "unspec" [
   UNSPEC_MXU2_BNEZ1Q
   UNSPEC_MXU2_BEQZ1Q
   UNSPEC_MXU2_BNEZ
   UNSPEC_MXU2_BEQZ
   UNSPEC_MXU2_CEQZ
   UNSPEC_MXU2_CNEZ
   UNSPEC_MXU2_CLEZ
   UNSPEC_MXU2_CLTZ
   UNSPEC_MXU2_AVES
   UNSPEC_MXU2_AVEU
   UNSPEC_MXU2_AVERS
   UNSPEC_MXU2_AVERU
   UNSPEC_MXU2_SUBUA
   UNSPEC_MXU2_SUBSS

   UNSPEC_MXU2_FMAX
   UNSPEC_MXU2_FMAXA
   UNSPEC_MXU2_FMIN
   UNSPEC_MXU2_FMINA
   UNSPEC_MXU2_FCLASS

   UNSPEC_MXU2_MAXA
   UNSPEC_MXU2_MAXS
   UNSPEC_MXU2_MAXU
   UNSPEC_MXU2_MINA
   UNSPEC_MXU2_MINS
   UNSPEC_MXU2_MINU
   UNSPEC_MXU2_SATS
   UNSPEC_MXU2_SATU
   UNSPEC_MXU2_SLLI
   UNSPEC_MXU2_SRAI
   UNSPEC_MXU2_SRLI
   UNSPEC_MXU2_SRAR
   UNSPEC_MXU2_SRARI
   UNSPEC_MXU2_SRLR
   UNSPEC_MXU2_SRLRI
   UNSPEC_MXU2_DOTPS
   UNSPEC_MXU2_DOTPU
   UNSPEC_MXU2_DADDS
   UNSPEC_MXU2_DADDU
   UNSPEC_MXU2_DSUBS
   UNSPEC_MXU2_DSUBU
   UNSPEC_MXU2_LOC
   UNSPEC_MXU2_LZC
   UNSPEC_MXU2_BCNT
   UNSPEC_MXU2_ANDIB
   UNSPEC_MXU2_NORIB
   UNSPEC_MXU2_ORIB
   UNSPEC_MXU2_XORIB
   UNSPEC_MXU2_NORV
   UNSPEC_MXU2_BSELV

   UNSPEC_MXU2_VCVTHS
   UNSPEC_MXU2_VCVTSD
   UNSPEC_MXU2_VCVTESH
   UNSPEC_MXU2_VCVTEDS
   UNSPEC_MXU2_VCVTOSH
   UNSPEC_MXU2_VCVTODS
   UNSPEC_MXU2_VCVTS_F
   UNSPEC_MXU2_VCVTU_F
   UNSPEC_MXU2_VCVTSWS
   UNSPEC_MXU2_VCVTSLD
   UNSPEC_MXU2_VCVTUWS
   UNSPEC_MXU2_VCVTULD
   UNSPEC_MXU2_VCVTRWS
   UNSPEC_MXU2_VCVTRLD
   UNSPEC_MXU2_VTRUNCS
   UNSPEC_MXU2_VTRUNCU
   UNSPEC_MXU2_VCVTQESH
   UNSPEC_MXU2_VCVTQEDW
   UNSPEC_MXU2_VCVTQOSH
   UNSPEC_MXU2_VCVTQODW
   UNSPEC_MXU2_VCVTQHS
   UNSPEC_MXU2_VCVTQWD

   UNSPEC_MXU2_MADDQ
   UNSPEC_MXU2_MADDQR
   UNSPEC_MXU2_MSUBQ
   UNSPEC_MXU2_MSUBQR
   UNSPEC_MXU2_MULQ
   UNSPEC_MXU2_MULQR

   UNSPEC_MXU2_MTFPU
   UNSPEC_MXU2_MFCPU
   UNSPEC_MXU2_MFFPU
   UNSPEC_MXU2_INSFCPU
   UNSPEC_MXU2_INSFFPU
   UNSPEC_MXU2_INSFMXU
   UNSPEC_MXU2_REPX
   UNSPEC_MXU2_REPI
   UNSPEC_MXU2_SHUFV
   UNSPEC_MXU2_SA1QX
   UNSPEC_MXU2_LA1QX
   UNSPEC_MXU2_SA1Q
   UNSPEC_MXU2_LA1Q
   UNSPEC_MXU2_SU1QX
   UNSPEC_MXU2_SU1Q
   UNSPEC_MXU2_LU1QX
   UNSPEC_MXU2_LU1Q
   UNSPEC_MXU2_CFCMXU
   UNSPEC_MXU2_CTCMXU
   UNSPEC_MXU2_FMADD
   UNSPEC_MXU2_FMSUB
])

;; This attribute is used to form the MODE for reg_or_0_operand
;; constraint.
(define_mode_attr REGOR0
  [(V2DF "DF")
   (V4SF "SF")
   (V2DI "DI")
   (V4SI "SI")
   (V8HI "SI")
   (V16QI "SI")])

;; This attribute qives define_insn suffix for instructions
;; with need distinction between integer and floating point.
(define_mode_attr mxu2fmt_f
  [(V2DF  "d_f")
   (V4SF  "w_f")
   (V2DI  "d")
   (V4SI  "w")
   (V8HI  "h")
   (V16QI "b")])

(define_mode_attr br_mxu2fmt
  [(V2DI  "2d")
   (V4SI  "4w")
   (V8HI  "8h")
   (V16QI "16b")])

(define_mode_attr mxu2fmt2
  [(V2DF  "d")
   (V4SF  "w")
   (V2DI  "d")
   (V4SI  "w")
   (V8HI  "h")
   (V16QI "b")])

(define_mode_attr mxu2fmt
  [(V2DF "d")
   (V4SF "s")
   (V2DI "d")
   (V4SI "w")
   (V8HI "h")
   (V16QI "b")])

;; All vector modes with 128 bits.
(define_mode_iterator MXU2     [V2DF V4SF V2DI V4SI V8HI V16QI])

;; Only integer modes.
(define_mode_iterator IMXU2     [V2DI V4SI V8HI V16QI])

;; As IMXU2 but excludeds V16QI.
(define_mode_iterator IMXU2_XQ [V2DI V4SI V8HI])

;; As IMXU2 but excludeds V2DI.
(define_mode_iterator IMXU2_X      [V4SI V8HI V16QI])

;; Only used with insert.
(define_mode_iterator MXU2_3    [V16QI V8HI])

;; Only floating-point modes.
(define_mode_iterator FMXU2     [V2DF V4SF])

(define_mode_attr FINT [(V4SF "V4SI") (V2DF "V2DI")])
(define_mode_attr RES [(V2DF "DF") (V4SF "SF") (V2DI "DI") (V4SI "SI") (V8HI "SI") (V16QI "SI")])
(define_mode_attr VHALFMODE [(V8HI "V16QI") (V4SI "V8HI") (V2DI "V4SI") (V2DF "V4SF")])
(define_mode_attr unitfmt [(V2DF "d") (V4SF "s")])
(define_mode_attr mxu2fmt_ivconv [(V2DF "l") (V4SF "w")])


;; Standard vector operation patterns for MXU2.
;; These enable C vector operations (a+b, a-b, etc.) and builtins
;; that map through CODE_FOR_addv4si3, etc.

(define_insn "mxu2_add<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(plus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		    (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "add<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_sub<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(minus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "sub<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_mul<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(mult:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		    (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "mul<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_div<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(div:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		   (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "divs<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_udiv<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(udiv:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		    (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "divu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_mod<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(mod:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		   (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "mods<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_umod<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(umod:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "modu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_and<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(and:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		   (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "andv\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_ior<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(ior:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		   (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "orv\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_xor<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(xor:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		   (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "xorv\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_one_cmpl<mode>2"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(not:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "norv\t%w0,%w1,%w1"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_neg<mode>2"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(neg:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "sub<mxu2fmt>\t%w0,%w0,%w1"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_vashl<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(ashift:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "sll<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_vashr<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(ashiftrt:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
			(match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "sra<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_vlshr<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(lshiftrt:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
			(match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "srl<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_smax<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(smax:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		    (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "maxs<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_smin<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(smin:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		    (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "mins<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_umax<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(umax:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "maxu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_umin<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(umin:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "minu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_ssadd<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(ss_plus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		       (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "addss<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_usadd<mode>3"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(us_plus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		       (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "adduu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

;; Float operations
(define_insn "mxu2_add<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(plus:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		    (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fadd<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_sub<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(minus:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		     (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fsub<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_mul<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(mult:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		    (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fmul<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_div<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(div:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		   (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fdiv<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_sqrt<mode>2"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(sqrt:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fsqrt<mxu2fmt>\t%w0,%w1"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_smax<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(smax:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		    (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fmax<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])

(define_insn "mxu2_smin<mode>3"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(smin:FMXU2 (match_operand:FMXU2 1 "register_operand" "q")
		    (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "fmin<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode" "TI")])


;; lat_mult1 for B/H 5; W 7; D 10;
;; lat_mult2 for B 12 H 20 W 36 D 68
;; lat_mult3 for float W 7 D 10
;; lat_mult4 for float W 10 D 15
;; lat_mult5 for float W 15 D 18

(define_attr "lat_type"
  "unknown, lat3, lat4, lat5, lat6, lat9, lat33,
   lat_mult1, lat_mult2, lat_mult3, lat_mult4,
   lat_mult5"
 (const_string "unknown"))

(define_attr "mxu2_execunit"
  "unknown, mxu2_eu_mult, mxu2_eu_div"
 (const_string "unknown"))

;;
;;Compare & Float point compare
;;
(define_code_iterator MXU2_FCC [ordered eq le lt])

(define_code_attr mxu2_fcc
    [(ordered   "fcor")
     (eq        "fceq")
     (le        "fcle")
     (lt        "fclt")])

(define_code_iterator MXU2_ICC [eq ne le leu lt ltu])

(define_code_attr mxu2_icc
    [(eq  "eq")
     (ne  "ne")
     (le  "les")
     (leu "leu")
     (lt  "lts")
     (ltu "ltu")])

;; 128bit MXU2 modes only in mxu2 registers or memmory
;; an exception is allowing MXU2 modes for GP registers for arguments
;; and return values.
(define_insn "mov<mode>_mxu2"
  [(set (match_operand:MXU2 0 "nonimmediate_operand" "=q,q,R,*d,q,q")
	(match_operand:MXU2 1 "move_operand" "q,R,q,q,*d,qYGYN"))]
  "ISA_HAS_MXU2
   && (register_operand (operands[0], <MODE>mode)
       || reg_or_0_operand (operands[1], <MODE>mode))"
{  return mips_output_move (operands[0], operands[1]); }
   [(set_attr "can_delay"    "yes,no,no,yes,yes,yes")
   (set_attr "mode"     "TI")])

(define_split
  [(set (match_operand:MXU2 0 "nonimmediate_operand")
	(match_operand:MXU2 1 "move_operand"))]
  "reload_completed && TARGET_MXU2
   && mips_split_128bit_move_p (operands[0], operands[1])"
  [(const_int 0)]
{
  mips_split_128bit_move (operands[0], operands[1]);
  DONE;
})

;;
;;Float point conversion
;;


(define_insn "mxu2_vcvths"
  [(set (match_operand:V8HI 0 "register_operand" "=q")
	(unspec:V8HI [(match_operand:V4SF 1 "register_operand" "q")
		      (match_operand:V4SF 2 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTHS))]
  "ISA_HAS_MXU2"
  "vcvths\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtsd"
  [(set (match_operand:V4SF 0 "register_operand" "=q")
	(unspec:V4SF [(match_operand:V2DF 1 "register_operand" "q")
		      (match_operand:V2DF 2 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTSD))]
  "ISA_HAS_MXU2"
  "vcvtsd\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtesh"
  [(set (match_operand:V4SF 0 "register_operand" "=q")
	(unspec:V4SF [(match_operand:V8HI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTESH))]
  "ISA_HAS_MXU2"
  "vcvtesh\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvteds"
  [(set (match_operand:V2DF 0 "register_operand" "=q")
	(unspec:V2DF [(match_operand:V4SF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTEDS))]
  "ISA_HAS_MXU2"
  "vcvteds\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtosh"
  [(set (match_operand:V4SF 0 "register_operand" "=q")
	(unspec:V4SF [(match_operand:V8HI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTOSH))]
  "ISA_HAS_MXU2"
  "vcvtosh\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtods"
  [(set (match_operand:V2DF 0 "register_operand" "=q")
	(unspec:V2DF [(match_operand:V4SF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTODS))]
  "ISA_HAS_MXU2"
  "vcvtods\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


;; int -> float vcvtssw/dl  vcvtusw/dl
(define_insn "mxu2_vcvts<FMXU2:unitfmt><FMXU2:mxu2fmt_ivconv>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:<FINT> 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTS_F))]
  "ISA_HAS_MXU2"
  "vcvts<FMXU2:unitfmt><FMXU2:mxu2fmt_ivconv>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtu<FMXU2:unitfmt><FMXU2:mxu2fmt_ivconv>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:<FINT> 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTU_F))]
  "ISA_HAS_MXU2"
  "vcvtu<FMXU2:unitfmt><FMXU2:mxu2fmt_ivconv>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;;vcvtsws/ vcvtsld
(define_insn "mxu2_vcvtsws"
  [(set (match_operand:V4SI 0 "register_operand" "=q")
	(unspec:V4SI [(match_operand:V4SF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTSWS))]
  "ISA_HAS_MXU2"
  "vcvtsws\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


(define_insn "mxu2_vcvtsld"
  [(set (match_operand:V2DI 0 "register_operand" "=q")
	(unspec:V2DI [(match_operand:V2DF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTSLD))]
  "ISA_HAS_MXU2"
  "vcvtsld\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;;vcvtuws/ vcvtuld
(define_insn "mxu2_vcvtuws"
  [(set (match_operand:V4SI 0 "register_operand" "=q")
	(unspec:V4SI [(match_operand:V4SF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTUWS))]
  "ISA_HAS_MXU2"
  "vcvtuws\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


(define_insn "mxu2_vcvtuld"
  [(set (match_operand:V2DI 0 "register_operand" "=q")
	(unspec:V2DI [(match_operand:V2DF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTULD))]
  "ISA_HAS_MXU2"
  "vcvtuld\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;; vcvtrws/ vcvtrld

(define_insn "mxu2_vcvtrws"
  [(set (match_operand:V4SI 0 "register_operand" "=q")
	(unspec:V4SI [(match_operand:V4SF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTRWS))]
  "ISA_HAS_MXU2"
  "vcvtrws\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


(define_insn "mxu2_vcvtrld"
  [(set (match_operand:V2DI 0 "register_operand" "=q")
	(unspec:V2DI [(match_operand:V2DF 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTRLD))]
  "ISA_HAS_MXU2"
  "vcvtrld\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;;vtruncsws/ vtruncsld
;;vtruncuws/ vtrunculd

(define_insn "mxu2_vtruncs<FMXU2:mxu2fmt_ivconv><FMXU2:unitfmt>"
  [(set (match_operand:<FINT> 0 "register_operand" "=q")
	(unspec:<FINT> [(match_operand:FMXU2 1 "register_operand" "q")]
		     UNSPEC_MXU2_VTRUNCS))]
  "ISA_HAS_MXU2"
  "vtruncs<FMXU2:mxu2fmt_ivconv><FMXU2:unitfmt>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vtruncu<FMXU2:mxu2fmt_ivconv><FMXU2:unitfmt>"
  [(set (match_operand:<FINT> 0 "register_operand" "=q")
	(unspec:<FINT> [(match_operand:FMXU2 1 "register_operand" "q")]
		     UNSPEC_MXU2_VTRUNCU))]
  "ISA_HAS_MXU2"
  "vtruncu<FMXU2:mxu2fmt_ivconv><FMXU2:unitfmt>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;;
;;vcvtqesh / vcvtqedw !!
;;
(define_insn "mxu2_vcvtqesh"
  [(set (match_operand:V4SF 0 "register_operand" "=q")
	(unspec:V4SF [(match_operand:V8HI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQESH))]
  "ISA_HAS_MXU2"
  "vcvtqesh\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


(define_insn "mxu2_vcvtqedw"
  [(set (match_operand:V2DF 0 "register_operand" "=q")
	(unspec:V2DF [(match_operand:V4SI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQEDW))]
  "ISA_HAS_MXU2"
  "vcvtqedw\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;;vcvtqosh/ vcvtqodw
(define_insn "mxu2_vcvtqosh"
  [(set (match_operand:V4SF 0 "register_operand" "=q")
	(unspec:V4SF [(match_operand:V8HI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQOSH))]
  "ISA_HAS_MXU2"
  "vcvtqosh\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


(define_insn "mxu2_vcvtqodw"
  [(set (match_operand:V2DF 0 "register_operand" "=q")
	(unspec:V2DF [(match_operand:V4SI 1 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQODW))]
  "ISA_HAS_MXU2"
  "vcvtqodw\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

;; vcvtqhs/ vcvtqwd
(define_insn "mxu2_vcvtqhs"
  [(set (match_operand:V8HI 0 "register_operand" "=q")
	(unspec:V8HI [(match_operand:V4SF 1 "register_operand" "q")
		      (match_operand:V4SF 2 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQHS))]
  "ISA_HAS_MXU2"
  "vcvtqhs\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])

(define_insn "mxu2_vcvtqwd"
  [(set (match_operand:V4SI 0 "register_operand" "=q")
	(unspec:V4SI [(match_operand:V2DF 1 "register_operand" "q")
		      (match_operand:V2DF 2 "register_operand" "q")]
		     UNSPEC_MXU2_VCVTQWD))]
  "ISA_HAS_MXU2"
  "vcvtqwd\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat9")])


;;
;; branch
;;

;;bnez1q
(define_insn "mxu2_branchnz_1q_<mxu2fmt_f>"
 [(set (pc) (if_then_else
       	        (ne (unspec:SI [(match_operand:MXU2 1 "register_operand" "q")]
	     	 	     UNSPEC_MXU2_BNEZ1Q)
	        (match_operand:SI 2 "const_0_operand"))
	     (label_ref (match_operand 0))
	     (pc)))]
  "ISA_HAS_MXU2"
  {
	return mips_output_conditional_branch (insn, operands,
	       				      MIPS_BRANCH ("bnez1q", "%w1,%0"),
	       				      MIPS_BRANCH ("beqz1q", "%w1,%0"));
  }
  [(set_attr "lat_type"	"lat3")])

(define_expand "mxu2_bnez1q_<mxu2fmt_f>"
 [(set (match_operand:SI 0 "register_operand" "=d")
       (unspec:SI [(match_operand:MXU2 1 "register_operand" "q")]
       UNSPEC_MXU2_BNEZ1Q))]
 "ISA_HAS_MXU2"
 {
	mips_expand_mxu2_branch (operands, gen_mxu2_branchnz_1q_<MXU2:mxu2fmt_f>);
	DONE;
 })

;;beqz1q
(define_insn "mxu2_branchz_1q_<mxu2fmt_f>"
 [(set (pc) (if_then_else
       	        (eq (unspec:SI [(match_operand:MXU2 1 "register_operand" "q")]
	     	 	     UNSPEC_MXU2_BEQZ1Q)
	        (match_operand:SI 2 "const_0_operand"))
	     (label_ref (match_operand 0))
	     (pc)))]
  "ISA_HAS_MXU2"
  {
	return mips_output_conditional_branch (insn, operands,
	       				      MIPS_BRANCH ("beqz1q", "%w1,%0"),
	       				      MIPS_BRANCH ("bnez1q", "%w1,%0"));
  }
  [(set_attr "lat_type"	"unknown")])

(define_expand "mxu2_beqz1q_<mxu2fmt_f>"
 [(set (match_operand:SI 0 "register_operand" "=d")
       (unspec:SI [(match_operand:MXU2 1 "register_operand" "q")]
       UNSPEC_MXU2_BEQZ1Q))]
 "ISA_HAS_MXU2"
 {
	mips_expand_mxu2_branch (operands, gen_mxu2_branchz_1q_<MXU2:mxu2fmt_f>);
	DONE;
 })

;; bnez<br_mxu2fmt>
(define_insn "mxu2_branchnz_<br_mxu2fmt>"
 [(set (pc) (if_then_else
       	        (ne (unspec:SI [(match_operand:IMXU2 1 "register_operand" "q")]
	     	 	     UNSPEC_MXU2_BNEZ)
	        (match_operand:IMXU2 2 "const_0_operand"))
	     (label_ref (match_operand 0))
	     (pc)))]
  "ISA_HAS_MXU2"
  {
	return mips_output_conditional_branch (insn, operands,
	       				      MIPS_BRANCH ("bnez<br_mxu2fmt>", "%w1,%0"),
	       				      MIPS_BRANCH ("beqz<br_mxu2fmt>", "%w1,%0"));
  }
  [(set_attr "lat_type"	"unknown")])

(define_expand "mxu2_bnez<br_mxu2fmt>"
 [(set (match_operand:SI 0 "register_operand" "=d")
       (unspec:SI [(match_operand:IMXU2 1 "register_operand" "q")]
       UNSPEC_MXU2_BNEZ))]
 "ISA_HAS_MXU2"
 {
	mips_expand_mxu2_branch (operands, gen_mxu2_branchnz_<IMXU2:br_mxu2fmt>);
	DONE;
 })


;; beqz<br_mxu2fmt>
(define_insn "mxu2_branchz_<br_mxu2fmt>"
 [(set (pc) (if_then_else
       	        (eq (unspec:SI [(match_operand:IMXU2 1 "register_operand" "q")]
	     	 	     UNSPEC_MXU2_BEQZ)
	        (match_operand:IMXU2 2 "const_0_operand"))
	     (label_ref (match_operand 0))
	     (pc)))]
  "ISA_HAS_MXU2"
  {
	return mips_output_conditional_branch (insn, operands,
	       				      MIPS_BRANCH ("beqz<br_mxu2fmt>", "%w1,%0"),
	       				      MIPS_BRANCH ("bnez<br_mxu2fmt>", "%w1,%0"));
  }
  [(set_attr "lat_type"	"unknown")])

(define_expand "mxu2_beqz<br_mxu2fmt>"
 [(set (match_operand:SI 0 "register_operand" "=d")
       (unspec:SI [(match_operand:IMXU2 1 "register_operand" "q")]
       UNSPEC_MXU2_BEQZ))]
 "ISA_HAS_MXU2"
 {
	mips_expand_mxu2_branch (operands, gen_mxu2_branchz_<IMXU2:br_mxu2fmt>);
	DONE;
 })

;;
;; Compare
;;

(define_insn "mxu2_c<MXU2_ICC:mxu2_icc>_<IMXU2:mxu2fmt2>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(MXU2_ICC:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
		  (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "c<MXU2_ICC:mxu2_icc><IMXU2:mxu2fmt2>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_ceqz_<mxu2fmt2>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
	UNSPEC_MXU2_CEQZ))]
  "ISA_HAS_MXU2"
  "ceqz<mxu2fmt2>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_cnez_<mxu2fmt2>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
	UNSPEC_MXU2_CNEZ))]
  "ISA_HAS_MXU2"
  "cnez<mxu2fmt2>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_clez_<mxu2fmt2>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
	UNSPEC_MXU2_CLEZ))]
  "ISA_HAS_MXU2"
  "clez<mxu2fmt2>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_cltz_<mxu2fmt2>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
	UNSPEC_MXU2_CLTZ))]
  "ISA_HAS_MXU2"
  "cltz<mxu2fmt2>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])


;;
;; integer arithmetic
;;

;; adda,addas,addss,adduu
(define_insn "mxu2_adda_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (plus:IMXU2 (abs:IMXU2 (match_operand:IMXU2 1 "register_operand" "q"))
       		  (abs:IMXU2 (match_operand:IMXU2 2 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "adda<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_addas_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (ss_plus:IMXU2 (abs:IMXU2 (match_operand:IMXU2 1 "register_operand" "q"))
       		  (abs:IMXU2 (match_operand:IMXU2 2 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "addas<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_addss_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (ss_plus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
          	     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "addss<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_adduu_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (us_plus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
       		     (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "adduu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

;;subsa,subua,subss,subuu,subus
(define_insn "mxu2_subsa_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (abs:IMXU2 (minus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
       		             (match_operand:IMXU2 2 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "subsa<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_subua_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
       		     (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_SUBUA))]
  "ISA_HAS_MXU2"
  "subua<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_subss_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
          	     (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_SUBSS))]
  "ISA_HAS_MXU2"
  "subss<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_subuu_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (us_minus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
       		      (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "subuu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_subus_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (ss_minus:IMXU2 (match_operand:IMXU2 1 "register_operand" "q")
       		      (match_operand:IMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "subus<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

;; aves,aveu avers,averu
(define_insn "mxu2_aves_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_AVES))]
  "ISA_HAS_MXU2"
  "aves<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_aveu_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_AVEU))]
  "ISA_HAS_MXU2"
  "aveu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_avers_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_AVERS))]
  "ISA_HAS_MXU2"
  "avers<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_averu_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_AVERU))]
  "ISA_HAS_MXU2"
  "averu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat3")])


;; madd,msub
(define_insn "mxu2_madd_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(plus:IMXU2 (mult:IMXU2 (match_operand:IMXU2 2 "register_operand" "q")
			      (match_operand:IMXU2 3 "register_operand" "q"))
		   (match_operand:IMXU2 1 "register_operand" "0")))]
  "ISA_HAS_MXU2"
  "madd<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "mxu2_execunit" "mxu2_eu_mult")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_msub_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(minus:IMXU2 (match_operand:IMXU2 1 "register_operand" "0")
		    (mult:IMXU2 (match_operand:IMXU2 2 "register_operand" "q")
			       (match_operand:IMXU2 3 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "msub<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "mxu2_execunit" "mxu2_eu_mult")
   (set_attr "lat_type" "lat_mult1")])

;; maxa,maxs,maxu,mina,mins,minu
(define_insn "mxu2_maxa_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MAXA))]
  "ISA_HAS_MXU2"
  "maxa<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_maxs_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MAXS))]
  "ISA_HAS_MXU2"
  "maxs<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_maxu_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MAXU))]
  "ISA_HAS_MXU2"
  "maxu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_mina_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MINA))]
  "ISA_HAS_MXU2"
  "mina<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_mins_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MINS))]
  "ISA_HAS_MXU2"
  "mins<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "lat6")])

(define_insn "mxu2_minu_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MINU))]
  "ISA_HAS_MXU2"
  "minu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "lat6")])

;;sats,stau
(define_insn "mxu2_sats_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand 2 "const_<bitimm>_operand" "")]
		     UNSPEC_MXU2_SATS))]
  "ISA_HAS_MXU2"
  "sats<mxu2fmt>\t%w0,%w1,%2"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_satu_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand 2 "const_<bitimm>_operand" "")]
		     UNSPEC_MXU2_SATU))]
  "ISA_HAS_MXU2"
  "satu<mxu2fmt>\t%w0,%w1,%2"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat4")])

;;
;; Dot Product
;;

;; dotps dotpu dadds daddu dsubs dsubu
(define_insn "mxu2_dotps_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<VHALFMODE> 1 "register_operand" "q")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")]
		       UNSPEC_MXU2_DOTPS))]
  "ISA_HAS_MXU2"
  "dotps<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_dotpu_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<VHALFMODE> 1 "register_operand" "q")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")]
		       UNSPEC_MXU2_DOTPU))]
  "ISA_HAS_MXU2"
  "dotpu<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_dadds_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<MODE> 1 "register_operand" "0")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")
			(match_operand:<VHALFMODE> 3 "register_operand" "q")]
		       UNSPEC_MXU2_DADDS))]
  "ISA_HAS_MXU2"
  "dadds<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_daddu_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<MODE> 1 "register_operand" "0")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")
			(match_operand:<VHALFMODE> 3 "register_operand" "q")]
		       UNSPEC_MXU2_DADDU))]
  "ISA_HAS_MXU2"
  "daddu<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_dsubs_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<MODE> 1 "register_operand" "0")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")
			(match_operand:<VHALFMODE> 3 "register_operand" "q")]
		       UNSPEC_MXU2_DSUBS))]
  "ISA_HAS_MXU2"
  "dsubs<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_dsubu_<mxu2fmt>"
  [(set (match_operand:IMXU2_XQ 0 "register_operand" "=q")
	(unspec:<MODE> [(match_operand:<MODE> 1 "register_operand" "0")
			(match_operand:<VHALFMODE> 2 "register_operand" "q")
			(match_operand:<VHALFMODE> 3 "register_operand" "q")]
		       UNSPEC_MXU2_DSUBU))]
  "ISA_HAS_MXU2"
  "dsubu<mxu2fmt>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat_mult1")])

;;
;; Bitwise
;;

;; loc,lzc,bcnt,andib,norv,norib,orib,xorvib,bselv
(define_insn "mxu2_loc_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
		       UNSPEC_MXU2_LOC))]
  "ISA_HAS_MXU2"
  "loc<mxu2fmt>\t%w0,%w1"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_lzc_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
		       UNSPEC_MXU2_LZC))]
  "ISA_HAS_MXU2"
  "lzc<mxu2fmt>\t%w0,%w1"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_bcnt_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")]
		       UNSPEC_MXU2_BCNT))]
  "ISA_HAS_MXU2"
  "bcnt<mxu2fmt>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])

(define_insn "mxu2_andib"
  [(set (match_operand:V16QI 0 "register_operand" "=q")
	(unspec:V16QI [(match_operand:V16QI 1 "register_operand" "q")
		      (match_operand  2 "const_uimm8_operand" )]
		       UNSPEC_MXU2_ANDIB))]
  "ISA_HAS_MXU2"
  "andib\t%w0,%w1,%2"
  [(set_attr "mode"     "TI")])

(define_insn "mxu2_norib"
  [(set (match_operand:V16QI 0 "register_operand" "=q")
	(unspec:V16QI [(match_operand:V16QI 1 "register_operand" "q")
		      (match_operand  2 "const_uimm8_operand" )]
		       UNSPEC_MXU2_NORIB))]
  "ISA_HAS_MXU2"
  "norib\t%w0,%w1,%2"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat3")])

(define_insn "mxu2_norv_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(and:IMXU2 ( not:IMXU2 (match_operand:IMXU2 1 "register_operand" "q"))
		   ( not:IMXU2 (match_operand:IMXU2 2 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "norv\t%w0,%w1,%w2"
  [(set_attr "alu_type"	"nor")
   (set_attr "mode"	"TI")
   (set_attr "lat_type" "lat3")]
)

(define_insn "mxu2_orib"
  [(set (match_operand:V16QI 0 "register_operand" "=q")
	(unspec:V16QI [(match_operand:V16QI 1 "register_operand" "q")
		      (match_operand  2 "const_uimm8_operand" )]
		       UNSPEC_MXU2_ORIB))]
  "ISA_HAS_MXU2"
  "orib\t%w0,%w1,%2"
  [(set_attr "mode"     "TI")])

(define_insn "mxu2_xorib"
  [(set (match_operand:V16QI 0 "register_operand" "=q")
	(unspec:V16QI [(match_operand:V16QI 1 "register_operand" "q")
		      (match_operand  2 "const_uimm8_operand" )]
		       UNSPEC_MXU2_XORIB))]
  "ISA_HAS_MXU2"
  "xorib\t%w0,%w1,%2"
  [(set_attr "type"	"arith")
   (set_attr "mode"	"TI")])

(define_insn "mxu2_bselv_<mxu2fmt>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(ior:IMXU2 (and:IMXU2 (not:IMXU2  (match_operand:IMXU2 3 "register_operand" "q"))
		   (match_operand:IMXU2 1 "register_operand" "q"))
		  (and:IMXU2 (match_dup 3)
		    (match_operand:IMXU2 2 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
 "bselv\t%w0,%w1,%w2,%w3"
 [(set_attr "type" "simd_bitmov")
  (set_attr "mode" "<MODE>")
  (set_attr "lat_type" "lat3")])

;;
;;Floating Point Arithmetic
;;

;; fmadd.fmsub,fmaxa,fmina,fclass
(define_insn "mxu2_fmadd_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:FMXU2 2 "register_operand" "q")
		       (match_operand:FMXU2 3 "register_operand" "q")
		       (match_operand:FMXU2 1 "register_operand" "0")]
		       UNSPEC_MXU2_FMADD))]
  "ISA_HAS_MXU2"
  "fmadd<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type"	"lat_mult5")])

(define_insn "*mxu2_fmadd_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(plus:FMXU2 (mult:FMXU2 (match_operand:FMXU2 2 "register_operand" "q")
			      (match_operand:FMXU2 3 "register_operand" "q"))
		   (match_operand:FMXU2 1 "register_operand" "0")))]
  "ISA_HAS_MXU2"
  "fmadd<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type"	"lat_mult5")])

(define_insn "mxu2_fmsub_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:FMXU2 1 "register_operand" "0")
		       (match_operand:FMXU2 2 "register_operand" "q")
		       (match_operand:FMXU2 3 "register_operand" "q")]
		       UNSPEC_MXU2_FMSUB))]
  "ISA_HAS_MXU2"
  "fmsub<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type"	"lat_mult5")])

(define_insn "*mxu2_fmsub_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(minus:FMXU2 (match_operand:FMXU2 1 "register_operand" "0")
		     (mult:FMXU2 (match_operand:FMXU2 2 "register_operand" "q")
			 (match_operand:FMXU2 3 "register_operand" "q"))))]
  "ISA_HAS_MXU2"
  "fmsub<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type"	"lat_mult5")])

(define_insn "mxu2_fmaxa_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:FMXU2 1 "register_operand" "q")
		      (match_operand:FMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_FMAXA))]
  "ISA_HAS_MXU2"
  "fmaxa<mxu2fmt2>\t%w0,%w1,%w2"
 [(set_attr "mode"	"TI")
  (set_attr "lat_type"	"lat6")])

(define_insn "mxu2_fmina_<mxu2fmt2>"
  [(set (match_operand:FMXU2 0 "register_operand" "=q")
	(unspec:FMXU2 [(match_operand:FMXU2 1 "register_operand" "q")
		      (match_operand:FMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_FMINA))]
  "ISA_HAS_MXU2"
  "fmina<mxu2fmt2>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type"	"lat6")])

(define_insn "mxu2_fclass_<mxu2fmt2>"
  [(set (match_operand:<VIMODE> 0 "register_operand" "=q")
	(unspec:<VIMODE> [(match_operand:FMXU2 1 "register_operand" "q")]
			 UNSPEC_MXU2_FCLASS))]
  "ISA_HAS_MXU2"
  "fclass<mxu2fmt2>\t%w0,%w1"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat3")])

;;
;; Floating point compare
;;

;; fceq fcle fclt fcor
(define_insn "mxu2_<MXU2_FCC:mxu2_fcc>_<FMXU2:mxu2fmt2>"
  [(set (match_operand:<VIMODE> 0 "register_operand" "=q")
	(MXU2_FCC:<VIMODE> (match_operand:FMXU2 1 "register_operand" "q")
		      (match_operand:FMXU2 2 "register_operand" "q")))]
  "ISA_HAS_MXU2"
  "<MXU2_FCC:mxu2_fcc><FMXU2:mxu2fmt2>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat4")])



;;
;; Fixed-point multiplication
;;

;;maddq maddqr msubq msubqr mulq mulqr

;; Only integer modes for fixed-point madd_q/maddr_q.
(define_mode_iterator QMXU2     [V4SI V8HI])

(define_insn "mxu2_maddq_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "0")
			  (match_operand:QMXU2 2 "register_operand" "q")
			  (match_operand:QMXU2 3 "register_operand" "q")]
		     UNSPEC_MXU2_MADDQ))]
  "ISA_HAS_MXU2"
  "maddq<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_maddqr_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "0")
			  (match_operand:QMXU2 2 "register_operand" "q")
			  (match_operand:QMXU2 3 "register_operand" "q")]
		     UNSPEC_MXU2_MADDQR))]
  "ISA_HAS_MXU2"
  "maddqr<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_msubq_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "0")
			  (match_operand:QMXU2 2 "register_operand" "q")
			  (match_operand:QMXU2 3 "register_operand" "q")]
		     UNSPEC_MXU2_MSUBQ))]
  "ISA_HAS_MXU2"
  "msubq<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_msubqr_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "0")
			  (match_operand:QMXU2 2 "register_operand" "q")
			  (match_operand:QMXU2 3 "register_operand" "q")]
		     UNSPEC_MXU2_MSUBQR))]
  "ISA_HAS_MXU2"
  "msubqr<mxu2fmt2>\t%w0,%w2,%w3"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_mulq_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "q")
		      (match_operand:QMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MULQ))]
  "ISA_HAS_MXU2"
  "mulq<mxu2fmt2>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

(define_insn "mxu2_mulqr_<mxu2fmt2>"
  [(set (match_operand:QMXU2 0 "register_operand" "=q")
	(unspec:QMXU2 [(match_operand:QMXU2 1 "register_operand" "q")
		      (match_operand:QMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_MULQR))]
  "ISA_HAS_MXU2"
  "mulqr<mxu2fmt2>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
   (set_attr "lat_type" "lat_mult1")])

;;
;; shift
;;

;; srar srari srlr srlri
(define_insn "mxu2_srar_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_SRAR))]
  "ISA_HAS_MXU2"
  "srar<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat4")])

(define_insn "mxu2_srari_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand 2 "const_<bitimm>_operand" "")]
		     UNSPEC_MXU2_SRARI))]
  "ISA_HAS_MXU2"
  "srari<mxu2fmt>\t%w0,%w1,%2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat4")])

(define_insn "mxu2_srlr_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand:IMXU2 2 "register_operand" "q")]
		     UNSPEC_MXU2_SRLR))]
  "ISA_HAS_MXU2"
  "srlr<mxu2fmt>\t%w0,%w1,%w2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat4")])

(define_insn "mxu2_srlri_<mxu2fmt_f>"
  [(set (match_operand:IMXU2 0 "register_operand" "=q")
	(unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
		      (match_operand 2 "const_<bitimm>_operand" "")]
		     UNSPEC_MXU2_SRLRI))]
  "ISA_HAS_MXU2"
  "srlri<mxu2fmt>\t%w0,%w1,%2"
  [(set_attr "mode"	"TI")
  (set_attr "lat_type" "lat4")])

;;
;; Element permute
;;

;; shufv
(define_insn "mxu2_shufv_<mxu2fmt_f>"
 [(set (match_operand:MXU2 0 "register_operand" "=q")
       (unspec:MXU2 [(match_operand:MXU2 1 "register_operand" "q")
       		    (match_operand:MXU2 2 "register_operand" "q")
		    (match_operand:MXU2 3 "register_operand" "q")]
		     UNSPEC_MXU2_SHUFV))]
 "ISA_HAS_MXU2"
 "shufv\t%w0,%w1,%w2,%w3"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

;;insfc(f)pu  -> insert
(define_insn "mxu2_insfcpu_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "0")
       	        (match_operand 2 "const_<indeximm>_operand" "")
		(match_operand:<REGOR0> 3 "reg_or_0_operand" "dJ")]
	       UNSPEC_MXU2_INSFCPU))]
 "ISA_HAS_MXU2"
 "insfcpu<mxu2fmt2>\t%w0[%2],%z3"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

; Similar to mxu2_ins*_<mxu2fmt> but with <UNITMODE>mode for operand 3.
;; Note that insert.d and insert.d_f will be split later if !TARGET_64BIT.

(define_insn "*mxu2_ins_<mxu2fmt_f>"
  [(set (match_operand:MXU2_3 0 "register_operand" "=q")
	(unspec:MXU2_3 [(match_operand:MXU2_3 1 "register_operand" "0")
		       (match_operand 2 "const_<indeximm>_operand" "")
		       (match_operand:<UNITMODE> 3 "reg_or_0_operand" "dJ")]
		      UNSPEC_MXU2_INSFCPU))]
  "ISA_HAS_MXU2"
  "insfcpu<mxu2fmt>\t%w0[%2],%z3"
  [(set_attr "mode"     "TI")
  (set_attr "lat_type" "unknown")])

(define_split
  [(set (match_operand:V2DI 0 "register_operand")
	(unspec:V2DI [(match_operand:V2DI 1 "register_operand")
		      (match_operand 2 "const_0_or_1_operand")
		      (match_operand:DI 3 "reg_or_0_operand")]
		     UNSPEC_MXU2_INSFCPU))]
  "reload_completed && ISA_HAS_MXU2"
  [(const_int 0)]
{
  mips_split_mxu2_insert_d (operands[0], operands[1], operands[2], operands[3]);
  DONE;
})

(define_insn "mxu2_insffpu_<mxu2fmt_f>"
 [(set (match_operand:FMXU2 0 "register_operand" "=q")
       (unspec:FMXU2 [(match_operand:FMXU2 1 "register_operand" "0")
       	        (match_operand 2 "const_<indeximm>_operand" "")
		(match_operand:<REGOR0> 3 "reg_or_0_operand" "f")]
	       UNSPEC_MXU2_INSFFPU))]
 "ISA_HAS_MXU2"
 "insffpu<mxu2fmt2>\t%w0[%2],%z3"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

;;insfmxu --> insve
(define_insn "mxu2_insfmxu_<mxu2fmt_f>"
 [(set (match_operand:MXU2 0 "register_operand" "=q")
       (unspec:MXU2 [(match_operand:MXU2 1 "register_operand" "0")
       	        (match_operand 2 "const_<indeximm>_operand" "")
		(match_operand:MXU2 3 "register_operand" "q")]
	       UNSPEC_MXU2_INSFMXU))]
 "ISA_HAS_MXU2"
 "insfmxu<mxu2fmt2>\t%w0[%2],%w3[0]"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

;;repx/i --> splat/i
(define_insn "mxu2_repx_<mxu2fmt_f>"
 [(set (match_operand:MXU2 0 "register_operand" "=q")
       (unspec:MXU2 [(match_operand:MXU2 1 "register_operand" "q")
       		     (match_operand:SI 2 "reg_or_0_operand" "dJ")]
		     UNSPEC_MXU2_REPX))]
 "ISA_HAS_MXU2"
 "repx<mxu2fmt2>\t%w0,%w1[%z2]"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

(define_insn "mxu2_repi_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (unspec:IMXU2 [(match_operand:IMXU2 1 "register_operand" "q")
       		     (match_operand 2 "const_<indeximm>_operand" "")]
		     UNSPEC_MXU2_REPI))]
 "ISA_HAS_MXU2"
 "repi<mxu2fmt2>\t%w0,%w1[%2]"
 [(set_attr "mode" "TI")
  (set_attr "lat_type" "lat3")])

;;
;; load/store
;;

;;
;; mtcpu(s/u)
;; Note that mtcpusd will be split later.
(define_insn "mxu2_mtcpus_<mxu2fmt_f>"
  [(set (match_operand:<RES> 0 "register_operand" "=d")
	(sign_extend:<RES>
	    (vec_select:<UNITMODE>
		(match_operand:IMXU2 1 "register_operand" "q")
		(parallel [(match_operand 2 "const_<indeximm>_operand" "")]))))]
  "ISA_HAS_MXU2"
  "mtcpus<mxu2fmt>\t%0,%w1[%2]"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat3")])

(define_split
  [(set (match_operand:<UNITMODE> 0 "register_operand")
	(sign_extend:<UNITMODE>
	    (vec_select:<UNITMODE>
		(match_operand:V2DI 1 "register_operand")
		(parallel [(match_operand 2 "const_0_or_1_operand")]))))]
  "reload_completed && TARGET_MXU2"
  [(const_int 0)]
{
  mips_split_mxu2_mtcpu_d (operands[0], operands[1], operands[2], gen_mxu2_mtcpus_w);
  DONE;
})
;; Note that mtcpuud will be split later.
(define_insn "mxu2_mtcpuu_<mxu2fmt_f>"
  [(set (match_operand:<RES> 0 "register_operand" "=d")
	(zero_extend:<RES>
	    (vec_select:<UNITMODE>
		(match_operand:IMXU2 1 "register_operand" "q")
		(parallel [(match_operand 2 "const_<indeximm>_operand" "")]))))]
  "ISA_HAS_MXU2"
  "mtcpuu<mxu2fmt2>\t%0,%w1[%2]"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat3")])

(define_split
  [(set (match_operand:<RES> 0 "register_operand")
	(zero_extend:<RES>
	    (vec_select:<RES>
		(match_operand:V2DI 1 "register_operand")
		(parallel [(match_operand 2 "const_0_or_1_operand")]))))]
  "reload_completed && TARGET_MXU2"
  [(const_int 0)]
{
  mips_split_mxu2_mtcpu_d (operands[0], operands[1], operands[2], gen_mxu2_mtcpuu_w);
  DONE;
})


;;mtfpu
(define_insn "mxu2_mtfpu_<mxu2fmt_f>"
  [(set (match_operand:<RES> 0 "register_operand" "=f")
	(unspec:<RES> [(match_operand:FMXU2 1 "register_operand" "q")
		       (match_operand 2 "const_<indeximm>_operand" "")]
		      UNSPEC_MXU2_MTFPU))]
  "ISA_HAS_MXU2"
  "mtfpu<mxu2fmt2>\t%0,%w1[%2]"
  [(set_attr "mode"     "TI")
   (set_attr "lat_type" "lat3")])

(define_split
  [(set (match_operand:MXU2 0 "nonimmediate_operand")
	(match_operand:MXU2 1 "move_operand"))]
  "reload_completed && TARGET_MXU2
   && mips_split_128bit_move_p (operands[0], operands[1])"
  [(const_int 0)]
{
  mips_split_128bit_move (operands[0], operands[1]);
  DONE;
})

;; mfcpu/ mffpu
(define_insn "mxu2_mfcpu_<mxu2fmt_f>"
 [(set (match_operand:IMXU2 0 "register_operand" "=q")
       (unspec:IMXU2 [(match_operand:<REGOR0> 1 "reg_or_0_operand" "dJ")]
       UNSPEC_MXU2_MFCPU))]
 "ISA_HAS_MXU2"
 "mfcpu<mxu2fmt2>\t%w0,%z1"
  [(set_attr "lat_type" "lat3")])


(define_split
  [(set (match_operand:V2DI 0 "register_operand")
	(unspec:V2DI [(match_operand:DI 1 "reg_or_0_operand")]
		     UNSPEC_MXU2_MFCPU))]
  "reload_completed && TARGET_MXU2"
  [(const_int 0)]
{
  mips_split_mxu2_mfcpu_d (operands[0], operands[1]);
  DONE;
})

(define_insn "mxu2_mffpu_<mxu2fmt_f>"
 [(set (match_operand:FMXU2 0 "register_operand" "=q")
       (unspec:FMXU2 [(match_operand:<REGOR0> 1 "reg_or_0_operand" "f")]
       UNSPEC_MXU2_MFFPU))]
 "ISA_HAS_MXU2"
 "mffpu<mxu2fmt2>\t%w0,%z1"
  [(set_attr "lat_type" "lat3")])

;;ctcmxu/cfcmxu
(define_insn "mxu2_ctcmxu"
  [(unspec_volatile [(match_operand 0 "const_uimm5_operand" "")
		     (match_operand:SI 1 "register_operand" "d")]
		    UNSPEC_MXU2_CTCMXU)]
  "ISA_HAS_MXU2"
  "ctcmxu\t$%0,%1"
  [(set_attr "mode"	"SI")
   (set_attr "lat_type"	"lat3")])


(define_insn "mxu2_cfcmxu"
  [(unspec_volatile [(match_operand:SI 0 "register_operand" "d")
  		     (match_operand 1 "const_uimm5_operand" "")]
		    UNSPEC_MXU2_CFCMXU)]
  "ISA_HAS_MXU2"
  "cfcmxu\t%0,$%1"
  [(set_attr "mode"	"SI")
   (set_attr "lat_type"	"lat3")])

;; lu1q,lu1qx
(define_insn "mxu2_lu1qx_<MXU2:mxu2fmt_f>"
  [(set (match_operand:MXU2 0 "register_operand"  "=q")
	(mem:MXU2 (plus:SI (match_operand 1 "register_operand" "d")
		(match_operand 2 "register_operand" "d"))))]
  "ISA_HAS_MXU2"
  "lu1qx\t%w0,%z2(%z1)"
  [(set_attr "lat_type"	"lat5")
  (set_attr "move_type" "load")
  (set_attr "mode" "TI")
  (set_attr "can_delay" "no")]
)

(define_insn "mxu2_lu1q_<MXU2:mxu2fmt_f>"
  [ (set (match_operand:MXU2 0 "register_operand" "=q")
	 (mem:MXU2 (plus:SI (match_operand:SI 1 "register_operand" "d")
		 (match_operand:SI 2 "const_imm10_operand"))))]
  "ISA_HAS_MXU2"
  "lu1q\t%w0,%2(%z1)"
  [(set_attr "lat_type"	"lat5")
  (set_attr "move_type" "load")
  (set_attr "can_delay" "no")
  (set_attr "mode" "TI")]
)

;; la1q,la1qx
(define_insn "mxu2_la1qx_<MXU2:mxu2fmt_f>"
  [(set (match_operand:MXU2 0 "register_operand"  "=q")
   (unspec:MXU2 [(mem:MXU2 (plus:SI (match_operand 1 "register_operand" "d")
			    (match_operand 2 "register_operand" "d")))]
   UNSPEC_MXU2_LA1QX))]
  "ISA_HAS_MXU2"
  "la1qx\t%w0,%z2(%z1)"
  [(set_attr "lat_type"	"lat5")
  (set_attr "move_type" "load")
  (set_attr "mode" "TI")])

(define_insn "mxu2_la1q_<MXU2:mxu2fmt_f>"
  [(set (match_operand:MXU2 0 "register_operand"  "=q")
   (unspec:MXU2 [(mem:MXU2 (plus:SI (match_operand:SI 1 "register_operand" "d")
			    (match_operand 2 "aq10_4_operand"  "")))]
    UNSPEC_MXU2_LA1Q))]
  "ISA_HAS_MXU2"
  "la1q\t%w0,%2(%z1)"
  [(set_attr "lat_type"	"lat5")
  (set_attr "move_type" "load")
  (set_attr "mode" "TI")])


;; Offset store
;; su1qx,su1q
(define_insn "mxu2_su1qx_<MXU2:mxu2fmt_f>"
  [(set (mem:MXU2 (plus:SI (match_operand 1 "register_operand" "d")
			   (match_operand 2 "register_operand" "d")))
		  (match_operand:MXU2 0 "register_operand"  "q"))]
  "ISA_HAS_MXU2"
  "su1qx\t%w0,%z2(%z1)"
  [(set_attr "lat_type" "unknown")
   (set_attr "mode" "TI")
   (set_attr "can_delay" "no")]
)

(define_insn "mxu2_su1q_<MXU2:mxu2fmt_f>"
  [(set (mem:MXU2 (plus:SI (match_operand:SI 1 "register_operand" "d")
	  (match_operand:SI 2 "const_imm10_operand" "YB")))
	(match_operand:MXU2 0 "register_operand"  "q"))]
  "ISA_HAS_MXU2"
  "su1q\t%w0,%2(%z1)"
  [(set_attr "lat_type" "unknown")
   (set_attr "mode" "TI")
   (set_attr "can_delay" "no")]
)

;; align 16 byte store
;; sa1qx,sa1q
(define_insn "mxu2_sa1qx_<MXU2:mxu2fmt_f>"
  [(set (mem:MXU2 (plus:SI (match_operand 1 "register_operand" "d")
   	              (match_operand 2 "register_operand" "d")))
        (unspec:MXU2 [(match_operand:MXU2 0 "register_operand"  "q")]
	UNSPEC_MXU2_SA1QX))]
  "ISA_HAS_MXU2"
  "sa1qx\t%w0,%z2(%z1)"
  [(set_attr "lat_type" "unknown")
   (set_attr "mode" "TI")])

(define_insn "mxu2_sa1q_<MXU2:mxu2fmt_f>"
  [(set (mem:MXU2 (plus:SI (match_operand 1 "register_operand" "d")
   	              (match_operand 2 "aq10_4_operand" "")))
        (unspec:MXU2 [(match_operand:MXU2 0 "register_operand"  "q")]
	UNSPEC_MXU2_SA1Q))]
  "ISA_HAS_MXU2"
  "sa1q\t%w0,%2(%z1)"
  [(set_attr "lat_type" "unknown")
   (set_attr "mode" "TI")])

;; li
(define_insn "mxu2_li<mode>_insn"
 [(set (match_operand:IMXU2 0  "register_operand" "=q")
       (match_operand:IMXU2 1  "const_vector_same_simm15_operand" ""))]
 "ISA_HAS_MXU2"
 {
    operands[1] = CONST_VECTOR_ELT (operands[1], 0);
    return "li<mxu2fmt>\t%w0,%d1";
 }
 [(set_attr "lat_type" "lat3")])


(define_expand "mxu2_li<mode>"
 [(match_operand:IMXU2 0 "register_operand")
  (match_operand 1 "const_imm15_operand")]
 "ISA_HAS_MXU2"
 {
  if (<MODE>mode == V16QImode)
    operands[1] = GEN_INT (trunc_int_for_mode (INTVAL (operands[1]),
					       <UNITMODE>mode));
  emit_insn (gen_mxu2_li<mode>_insn (operands[0],
		  mips_gen_const_int_vector (<MODE>mode, INTVAL (operands[1]))));
  DONE;
 })

(define_insn "mxu2_li0<mode>"
 [(set (match_operand:FMXU2 0 "register_operand" "=q")
       (match_operand 1 "const_0_operand"))]
  "ISA_HAS_MXU2"
  "li<mxu2fmt2>\t%w0,0"
  [(set_attr "mode" "TI")
   (set_attr "lat_type"	"lat3")])

