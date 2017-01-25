# Copyright 2016 Google Inc. All rights reserved.
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
cmake_minimum_required(VERSION 2.8.7)

# Hyphenation pattern files.
set (PATTERN_FILES_SRC
     as/hyph-as.pat.txt
     bn/hyph-bn.pat.txt
     cy/hyph-cy.pat.txt
     da/hyph-da.pat.txt
     de/hyph-de-1901.pat.txt
     de/hyph-de-1996.pat.txt
     de/hyph-de-ch-1901.pat.txt
     en-GB/hyph-en-gb.pat.txt
     en-US/hyph-en-us.pat.txt
     es/hyph-es.pat.txt
     et/hyph-et.pat.txt
     eu/hyph-eu.pat.txt
     Ethi/hyph-und-ethi.pat.txt
     fr/hyph-fr.pat.txt
     ga/hyph-ga.pat.txt
     gu/hyph-gu.pat.txt
     hi/hyph-hi.pat.txt
     hr/hyph-hr.pat.txt
     hu/hyph-hu.pat.txt
     hy/hyph-hy.pat.txt
     kn/hyph-kn.pat.txt
     ml/hyph-ml.pat.txt
     mn/hyph-mn-cyrl.pat.txt
     mr/hyph-mr.pat.txt
     nb/hyph-nb.pat.txt
     nn/hyph-nn.pat.txt
     or/hyph-or.pat.txt
     pa/hyph-pa.pat.txt
     pt/hyph-pt.pat.txt
     sl/hyph-sl.pat.txt
     ta/hyph-ta.pat.txt
     te/hyph-te.pat.txt
     tk/hyph-tk.pat.txt
)

set(hyb_dir ${dependencies_hyb_distr_dir}/)
set(hyb_destination_dir ${flatui_assets_dir}/hyphen-data/)
set(hyb_script_name "mk_hyb_file.py")

# Create destination directory, since script does not.
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory
                    ${hyb_destination_dir})

# Convert patten files into hyb binary.
set (python_cmd "python")

foreach(PATTERN_FILE ${PATTERN_FILES_SRC})
  # Generate target file name.
  STRING(REGEX REPLACE "^[a-zA-Z-]+/" "" DEST_NAME ${PATTERN_FILE})
  STRING(REGEX REPLACE ".pat.txt$" ".hyb" DEST_NAME ${DEST_NAME})

  # Generate python arguments and execute the command.
  if(${hyb_dir}${PATTERN_FILE} IS_NEWER_THAN ${hyb_destination_dir}${DEST_NAME})
    set(python_arg ${hyb_script_name} ${hyb_dir}${PATTERN_FILE}
        ${hyb_destination_dir}${DEST_NAME})
    message("Converting hyb: " ${PATTERN_FILE} " to " ${DEST_NAME})
    execute_process(COMMAND ${python_cmd} ${python_arg}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      RESULT_VARIABLE python_result)
  endif()
endforeach(PATTERN_FILE)


