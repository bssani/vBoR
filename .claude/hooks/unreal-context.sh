#!/usr/bin/env bash
# SessionStart hook: inject a short note identifying the session as operating inside
# an Unreal Engine project, plus the few project facts that are costly to rediscover.
#
# Walks upward from $PWD so sessions started in subdirectories (Source/, Content/,
# Plugins/, ...) are still detected. Every appended line is gated on a signal we
# actually detected, so this stays correct as the project changes.
#
# Cloud sessions (claude.ai/code, the Claude app) set CLAUDE_CODE_REMOTE=true. Those VMs
# have no Unreal Engine, so they get "don't build, leave a verify-locally list" instead of
# the build command.
#
# Adapted from Epic Games' unreal-engine-skills-for-claude-code-plugin (MIT), via GMTCK_MR.
# Opt-in debug logging: set CLAUDE_UE_HOOK_DEBUG to any non-empty value.

debug() {
  if [ -n "$CLAUDE_UE_HOOK_DEBUG" ]; then
    echo "unreal-context.sh: $*" >&2
  fi
}

is_project_root() {
  # Reliable top-level markers only. A bare `Engine` directory is NOT a marker:
  # the Unreal source tree contains an `Engine` module at
  # `Engine/Source/Runtime/Engine`, so walking up from a Runtime module would
  # otherwise short-circuit and mis-identify it as the root.
  local directory="$1"
  [ -f "$directory/GenerateProjectFiles.bat" ] && return 0
  [ -f "$directory/GenerateProjectFiles.sh" ] && return 0
  for candidate in "$directory"/*.uproject; do
    [ -e "$candidate" ] && return 0
  done
  return 1
}

find_project_root() {
  local directory="$1"
  while [ -n "$directory" ]; do
    if is_project_root "$directory"; then
      echo "$directory"
      return 0
    fi
    local parent
    parent="$(dirname "$directory")"
    [ "$parent" = "$directory" ] && break
    directory="$parent"
  done
  return 1
}

project_root="$(find_project_root "$PWD")"
if [ -z "$project_root" ]; then
  debug "no Unreal Engine project marker found walking up from $PWD"
  exit 0
fi

uproject_filename=""
for candidate in "$project_root"/*.uproject; do
  if [ -e "$candidate" ]; then
    uproject_filename="$(basename "$candidate")"
    break
  fi
done

# Engine version straight from the .uproject, so this never goes stale.
engine_version=""
if [ -n "$uproject_filename" ]; then
  engine_version="$(sed -n 's/.*"EngineAssociation"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
    "$project_root/$uproject_filename" 2>/dev/null | head -1)"
fi

target_name=""
for candidate in "$project_root"/Source/*.Target.cs; do
  if [ -e "$candidate" ]; then
    target_name="$(basename "$candidate" .Target.cs)"
    break
  fi
done

metaxr_present="false"
[ -d "$project_root/Plugins/MetaXR" ] && metaxr_present="true"

agent_present="false"
[ -d "$project_root/Plugins/UnrealAgent" ] && agent_present="true"

is_cloud="false"
[ "$CLAUDE_CODE_REMOTE" = "true" ] && is_cloud="true"

debug "root=$project_root uproject=$uproject_filename engine=$engine_version metaxr=$metaxr_present agent=$agent_present cloud=$is_cloud"

# --- Build the injected context. Keep it short. ---
context="This working directory is an Unreal Engine project"
[ -n "$uproject_filename" ] && context="$context (\`$uproject_filename\`"
[ -n "$uproject_filename" ] && [ -n "$engine_version" ] && context="$context, engine $engine_version"
[ -n "$uproject_filename" ] && context="$context)"
context="$context. Prefer Unreal Engine conventions (C++/UObject patterns, Slate, UHT reflection) when suggesting code."

if [ "$is_cloud" = "true" ]; then
  context="$context CLOUD SESSION: this VM has no Unreal Engine, so nothing can be compiled, run, or opened in the editor."
  context="$context Do not attempt a build and do not edit binary assets (.uasset/.umap). Write C++ and docs only,"
  context="$context run the ue-compile-check agent on changed C++ before committing, and end with a 'verify locally' list:"
  context="$context what needs a build, what needs editor work (Blueprints, assets, levels), and what needs a headset test."
elif [ -n "$target_name" ]; then
  context="$context Build with \`Engine/Build/BatchFiles/Build.bat ${target_name}Editor Win64 Development -Project=<abs .uproject>\`."
fi

if [ "$metaxr_present" = "false" ]; then
  # The Meta XR plugin is normally committed; this only fires if a checkout lacks it.
  context="$context The Meta XR plugin (OculusXR* modules) is not in this checkout, so its API cannot be read here."
  context="$context Do not write OculusXR calls from memory: prefer engine-level XR APIs (OpenXR / HeadMountedDisplay),"
  context="$context and if a Meta-specific call is unavoidable, isolate it in one small file and flag it for local verification."
fi

if [ "$is_cloud" = "false" ]; then
  # The single most expensive local mistake: git operations while the editor holds .uasset locks.
  context="$context Close the Unreal Editor before any git branch switch, merge, or C++ rebuild -- the editor locks"
  context="$context \`.uasset\` files and a mid-operation failure leaves the working tree half-written."
fi

if [ "$agent_present" = "true" ] && [ "$is_cloud" = "false" ]; then
  context="$context The UnrealAgent plugin provides an in-editor MCP server (editor ops via Python and toolsets);"
  context="$context it is only reachable while the editor is running, so C++ rebuilds and editor-driven work cannot overlap."
fi

# JSON-escape the dynamic content: backslash first, then double quote.
escaped_context="${context//\\/\\\\}"
escaped_context="${escaped_context//\"/\\\"}"

printf '{"hookSpecificOutput":{"hookEventName":"SessionStart","additionalContext":"%s"}}\n' "$escaped_context"
