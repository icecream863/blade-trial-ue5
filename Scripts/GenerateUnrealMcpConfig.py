"""Generate or validate the UE 5.8 project-local Codex MCP configuration."""

from pathlib import Path

import unreal


config_path = Path(unreal.Paths.project_dir()) / ".codex" / "config.toml"
if not config_path.is_file():
    unreal.SystemLibrary.execute_console_command(
        None, "ModelContextProtocol.GenerateClientConfig Codex"
    )
    if not config_path.is_file():
        raise RuntimeError("UE did not create the project-local Codex MCP config: " + str(config_path))

contents = config_path.read_text(encoding="utf-8")
if "[mcp_servers.unreal-mcp]" not in contents or "http://127.0.0.1:8000/mcp" not in contents:
    raise RuntimeError("Generated Codex MCP config is missing the UE 5.8 local server entry")

unreal.log("OFFICIAL_UNREAL_MCP_CONFIG_VERIFIED " + str(config_path))
