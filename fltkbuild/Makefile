#####################################################
# Makefile for the bundled FLTK library             #
#####################################################
# The FLTK sources live in $(TOP)/fltk and are      #
# configured by Grace's configure. Only the core    #
# library (libfltk.a) is built; nothing of FLTK is  #
# installed.                                        #
#####################################################

TOP=..

include $(TOP)/Make.conf

FLTK_DIR=$(TOP)/fltk

all : $(FLTK_DIR)/lib/libfltk.a

$(FLTK_DIR)/lib/libfltk.a : $(FLTK_DIR)/makeinclude
	cd $(FLTK_DIR)/src && $(MAKE) ../lib/libfltk.a

install :

tests :

links :

clean :
	cd $(FLTK_DIR)/src && $(MAKE) clean

distclean : clean
	$(RM) $(FLTK_DIR)/makeinclude $(FLTK_DIR)/config.h \
	$(FLTK_DIR)/FL/fl_config.h $(FLTK_DIR)/fltk-config \
	$(FLTK_DIR)/config.log $(FLTK_DIR)/config.status \
	$(FLTK_DIR)/config.grace.log

devclean : distclean

dummy :
