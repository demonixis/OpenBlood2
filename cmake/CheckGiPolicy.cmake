cmake_minimum_required(VERSION 3.28)

# Decision D3 (gi_contracts.md lot G6): placement and settings are committed, baked GI files never
# are. Fails when the index holds a .ob2gi file, anything under data/gi/baked or the bake report,
# or when .gitignore stops ignoring them. Run with -DSOURCE=<repository>; outside a git checkout
# (a source archive) there is nothing to check and ctest reports a skip (SKIP_REGULAR_EXPRESSION).
if(NOT SOURCE)
  message(FATAL_ERROR "Set SOURCE to the repository root")
endif()
find_program(git_program git)
if(NOT git_program OR NOT EXISTS "${SOURCE}/.git")
  message("GI_POLICY skipped: not a git checkout")
  return()
endif()
execute_process(COMMAND "${git_program}" -C "${SOURCE}" ls-files --cached -- "*.ob2gi" "data/gi/baked"
                        "data/gi/bake_report.json"
  OUTPUT_VARIABLE tracked RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "git ls-files failed (${result})")
endif()
string(STRIP "${tracked}" tracked)
if(tracked)
  message(FATAL_ERROR "Baked GI files are committed (decision D3):\n${tracked}")
endif()
foreach(path IN ITEMS data/gi/baked/worlds/probe.ob2gi data/gi/bake_report.json)
  execute_process(COMMAND "${git_program}" -C "${SOURCE}" check-ignore --no-index -q "${path}"
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR ".gitignore does not ignore ${path} (decision D3)")
  endif()
endforeach()
message("GI_POLICY ok")
