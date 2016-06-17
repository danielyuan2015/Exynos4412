#
# (C) Copyright 2000, 2001, 2002
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# (C) Copyright 2008
# Guennadi Liakhovetki, DENX Software Engineering, <lg@denx.de>
#
# (C) Copyright 2011 Samsung Electronics Co. Ltd
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

include $(TOPDIR)/config.mk

LIB	= $(obj)lib$(BOARD).o

COBJS-y	:= x4412.o
COBJS-y	+= pmic.o
COBJS-y	+= smc.o
SOBJS	:= lowlevel_init.o
SOBJS	+= mem_init_x4412.o
SOBJS	+= clock_init_x4412.o

COBJS-y	+= color.o maps.o rect.o pixel.o surface.o malloc.o font.o
COBJS-y	+= sw_blit.o sw_blit_alpha.o sw_blit_replace.o sw_fill.o sw_hline.o sw_point.o sw_rotate.o sw_scale.o sw_utils.o sw_vline.o

COBJS-y	+= exynos4412-clk.o
COBJS-y	+= exynos4412-fb.o

SRCS    := $(SOBJS:.o=.S) $(COBJS-y:.o=.c)
OBJS	:= $(addprefix $(obj),$(COBJS-y))
SOBJS	:= $(addprefix $(obj),$(SOBJS))

$(LIB):	$(obj).depend $(SOBJS) $(OBJS)
	$(call cmd_link_o_target, $(SOBJS) $(OBJS))

clean:
	rm -f $(SOBJS) $(OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak $(obj).depend

#########################################################################

# defines $(obj).depend target
include $(SRCTREE)/rules.mk

sinclude $(obj).depend

#########################################################################
