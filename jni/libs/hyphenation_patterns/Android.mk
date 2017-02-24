# Copyright 2017 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:=$(call my-dir)

# Project directory relative to this file.
HYB_DEST_DIR := $(call realpath-portable, $(LOCAL_PATH)/../../../assets/hyphen-data)
PATTERN_SRC_DIR := $(call realpath-portable, $(DEPENDENCIES_HYPHENATION_PATTERN_DIR))
SCRIPT_PATH := $(call realpath-portable, $(LOCAL_PATH)/../../../cmake/hyphenation_patterns)
CMD := python $(SCRIPT_PATH)/mk_hyb_file.py

$(HYB_DEST_DIR)/hyph-as.hyb::$(PATTERN_SRC_DIR)/as/hyph-as.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-bn.hyb::$(PATTERN_SRC_DIR)/bn/hyph-bn.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-cy.hyb::$(PATTERN_SRC_DIR)/cy/hyph-cy.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-da.hyb::$(PATTERN_SRC_DIR)/da/hyph-da.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-de-1901.hyb::$(PATTERN_SRC_DIR)/de/hyph-de-1901.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-de-1996.hyb::$(PATTERN_SRC_DIR)/de/hyph-de-1996.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-de-ch-1901.hyb::$(PATTERN_SRC_DIR)/de/hyph-de-ch-1901.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-en-gb.hyb::$(PATTERN_SRC_DIR)/en-GB/hyph-en-gb.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-en-us.hyb::$(PATTERN_SRC_DIR)/en-US/hyph-en-us.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-es.hyb::$(PATTERN_SRC_DIR)/es/hyph-es.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-et.hyb::$(PATTERN_SRC_DIR)/et/hyph-et.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-eu.hyb::$(PATTERN_SRC_DIR)/eu/hyph-eu.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-fr.hyb::$(PATTERN_SRC_DIR)/fr/hyph-fr.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-ga.hyb::$(PATTERN_SRC_DIR)/ga/hyph-ga.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-gu.hyb::$(PATTERN_SRC_DIR)/gu/hyph-gu.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-hi.hyb::$(PATTERN_SRC_DIR)/hi/hyph-hi.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-hr.hyb::$(PATTERN_SRC_DIR)/hr/hyph-hr.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-hu.hyb::$(PATTERN_SRC_DIR)/hu/hyph-hu.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-hy.hyb::$(PATTERN_SRC_DIR)/hy/hyph-hy.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-kn.hyb::$(PATTERN_SRC_DIR)/kn/hyph-kn.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-ml.hyb::$(PATTERN_SRC_DIR)/ml/hyph-ml.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-mn-cyrl.hyb::$(PATTERN_SRC_DIR)/mn/hyph-mn-cyrl.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-mr.hyb::$(PATTERN_SRC_DIR)/mr/hyph-mr.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-nb.hyb::$(PATTERN_SRC_DIR)/nb/hyph-nb.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-nn.hyb::$(PATTERN_SRC_DIR)/nn/hyph-nn.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-or.hyb::$(PATTERN_SRC_DIR)/or/hyph-or.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-pa.hyb::$(PATTERN_SRC_DIR)/pa/hyph-pa.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-pt.hyb::$(PATTERN_SRC_DIR)/pt/hyph-pt.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-sl.hyb::$(PATTERN_SRC_DIR)/sl/hyph-sl.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-ta.hyb::$(PATTERN_SRC_DIR)/ta/hyph-ta.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-te.hyb::$(PATTERN_SRC_DIR)/te/hyph-te.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-tk.hyb::$(PATTERN_SRC_DIR)/tk/hyph-tk.pat.txt
	$(CMD) $^ $@
$(HYB_DEST_DIR)/hyph-und-ethi.hyb::$(PATTERN_SRC_DIR)/Ethi/hyph-und-ethi.pat.txt
	$(CMD) $^ $@

all:$(HYB_DEST_DIR)/hyph-as.hyb \
$(HYB_DEST_DIR)/hyph-bn.hyb \
$(HYB_DEST_DIR)/hyph-cy.hyb \
$(HYB_DEST_DIR)/hyph-da.hyb \
$(HYB_DEST_DIR)/hyph-de-1901.hyb \
$(HYB_DEST_DIR)/hyph-de-1996.hyb \
$(HYB_DEST_DIR)/hyph-de-ch-1901.hyb \
$(HYB_DEST_DIR)/hyph-en-gb.hyb \
$(HYB_DEST_DIR)/hyph-en-us.hyb \
$(HYB_DEST_DIR)/hyph-es.hyb \
$(HYB_DEST_DIR)/hyph-et.hyb \
$(HYB_DEST_DIR)/hyph-eu.hyb \
$(HYB_DEST_DIR)/hyph-fr.hyb \
$(HYB_DEST_DIR)/hyph-ga.hyb \
$(HYB_DEST_DIR)/hyph-gu.hyb \
$(HYB_DEST_DIR)/hyph-hi.hyb \
$(HYB_DEST_DIR)/hyph-hr.hyb \
$(HYB_DEST_DIR)/hyph-hu.hyb \
$(HYB_DEST_DIR)/hyph-hy.hyb \
$(HYB_DEST_DIR)/hyph-kn.hyb \
$(HYB_DEST_DIR)/hyph-ml.hyb \
$(HYB_DEST_DIR)/hyph-mn-cyrl.hyb \
$(HYB_DEST_DIR)/hyph-mr.hyb \
$(HYB_DEST_DIR)/hyph-nb.hyb \
$(HYB_DEST_DIR)/hyph-nn.hyb \
$(HYB_DEST_DIR)/hyph-or.hyb \
$(HYB_DEST_DIR)/hyph-pa.hyb \
$(HYB_DEST_DIR)/hyph-pt.hyb \
$(HYB_DEST_DIR)/hyph-sl.hyb \
$(HYB_DEST_DIR)/hyph-ta.hyb \
$(HYB_DEST_DIR)/hyph-te.hyb \
$(HYB_DEST_DIR)/hyph-tk.hyb \
$(HYB_DEST_DIR)/hyph-und-ethi.hyb
