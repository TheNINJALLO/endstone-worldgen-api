from __future__ import annotations

import ast
import logging
from pathlib import Path
from types import ModuleType, SimpleNamespace
import sys
import tomllib
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_PROJECT = ROOT / "examples" / "python" / "world_gen_studio_plugin"
PLUGIN_SOURCE = PLUGIN_PROJECT / "src"


def install_endstone_test_double() -> None:
    endstone_module = ModuleType("endstone")
    command_module = ModuleType("endstone.command")
    plugin_module = ModuleType("endstone.plugin")

    class Plugin:
        def __init__(self) -> None:
            self.server = object()
            self.logger = logging.getLogger(type(self).__name__)

    class Command:
        def __init__(self, name: str):
            self.name = name

    class CommandSender:
        pass

    plugin_module.Plugin = Plugin
    command_module.Command = Command
    command_module.CommandSender = CommandSender
    endstone_module.plugin = plugin_module
    endstone_module.command = command_module
    sys.modules["endstone"] = endstone_module
    sys.modules["endstone.plugin"] = plugin_module
    sys.modules["endstone.command"] = command_module


install_endstone_test_double()
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(PLUGIN_SOURCE))

from endstone_worldgen_studio import WorldGenStudioPlugin
from endstone_worldgen_studio import _bridge_loader as bridge_loader
from endstone_worldgen import GenerationScheduler


class FakeSender:
    def __init__(self) -> None:
        self.location = SimpleNamespace(
            x=32,
            y=70,
            z=-16,
            dimension=SimpleNamespace(name="overworld"),
        )
        self.messages: list[str] = []

    def send_message(self, message: str) -> None:
        self.messages.append(message)


class FakeLiveBridge:
    def __init__(self) -> None:
        self.available_now = True
        self.availability_checks = 0

    def available(self, server):
        del server
        self.availability_checks += 1
        return self.available_now

    def status(self, server):
        del server
        return {
            "interception_active": True,
            "populator_count": 2,
            "stats": {
                "dispatched": 10,
                "committed": 8,
                "waiting": 1,
                "inflight": 1,
                "capture_retries": 0,
            },
            "diagnostics": {
                "intercepted_requests": 12,
                "dropped_requests": 0,
            },
        }


class StrictLogger:
    """Match Endstone's one-positional-string logger methods."""

    def __init__(self) -> None:
        self.errors: list[str] = []
        self.infos: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def info(self, message: str) -> None:
        self.infos.append(message)


def missing_module(name: str) -> ModuleNotFoundError:
    return ModuleNotFoundError(f"No module named {name!r}", name=name)


class StudioWheelTests(unittest.TestCase):
    def make_plugin(self) -> tuple[WorldGenStudioPlugin, FakeSender]:
        plugin = WorldGenStudioPlugin()
        plugin.scheduler = GenerationScheduler(workers=2)
        plugin.live_bridge = FakeLiveBridge()
        plugin.bridge_error = ""
        self.addCleanup(plugin.on_disable)
        return plugin, FakeSender()

    def test_packaging_uses_current_endstone_entry_point(self) -> None:
        metadata = tomllib.loads((PLUGIN_PROJECT / "pyproject.toml").read_text("utf-8"))
        project = metadata["project"]
        self.assertEqual(project["requires-python"], "==3.14.*")
        self.assertEqual(project["dependencies"], ["endstone==0.11.6"])
        self.assertEqual(
            project["entry-points"]["endstone"],
            {"worldgen-studio": "endstone_worldgen_studio:WorldGenStudioPlugin"},
        )
        self.assertNotIn("endstone.plugins", project["entry-points"])
        self.assertEqual(
            set(metadata["tool"]["setuptools"]["packages"]),
            {"endstone_worldgen_studio", "endstone_worldgen"},
        )

    def test_all_commands_permissions_and_usages_are_declared(self) -> None:
        self.assertEqual(WorldGenStudioPlugin.api_version, "0.11")
        self.assertEqual(WorldGenStudioPlugin.depend, ["worldgen_api"])
        self.assertEqual(set(WorldGenStudioPlugin.commands), {"wg"})
        command = WorldGenStudioPlugin.commands["wg"]
        self.assertEqual(command["permissions"], ["wg.admin"])
        self.assertEqual(
            set(WorldGenStudioPlugin._SUBCOMMAND_HANDLERS),
            {"gen", "structure", "benchmark", "inspect", "status"},
        )
        usages = "\n".join(command["usages"])
        for subcommand in WorldGenStudioPlugin._SUBCOMMAND_HANDLERS:
            self.assertIn(f"({subcommand})", usages)
        self.assertNotIn("[args...]", usages)
        self.assertEqual(WorldGenStudioPlugin.permissions["wg.admin"]["default"], "op")

    def test_endstone_logger_calls_pass_one_rendered_string(self) -> None:
        plugin_source = (
            PLUGIN_SOURCE / "endstone_worldgen_studio" / "plugin.py"
        ).read_text("utf-8")
        tree = ast.parse(plugin_source)
        logger_calls = [
            node
            for node in ast.walk(tree)
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and isinstance(node.func.value, ast.Attribute)
            and isinstance(node.func.value.value, ast.Name)
            and node.func.value.value.id == "self"
            and node.func.value.attr == "logger"
        ]
        self.assertEqual(len(logger_calls), 2)
        for call in logger_calls:
            self.assertEqual(len(call.args), 1)
            self.assertEqual(call.keywords, [])

        plugin = WorldGenStudioPlugin()
        strict_logger = StrictLogger()
        plugin.logger = strict_logger
        with patch(
            "endstone_worldgen_studio.plugin.import_live_bridge",
            side_effect=missing_module("_endstone_worldgen_live"),
        ):
            plugin.on_enable()
        self.assertEqual(len(strict_logger.errors), 1)
        self.assertIn("_endstone_worldgen_live", strict_logger.errors[0])

        enabled_plugin = WorldGenStudioPlugin()
        enabled_logger = StrictLogger()
        enabled_plugin.logger = enabled_logger
        with patch(
            "endstone_worldgen_studio.plugin.import_live_bridge",
            return_value=FakeLiveBridge(),
        ):
            enabled_plugin.on_enable()
        self.addCleanup(enabled_plugin.on_disable)
        self.assertEqual(
            enabled_logger.infos,
            ["WorldGen Studio enabled against the native endstone:worldgen service."],
        )

    def test_bridge_loader_prefers_package_relative_companion(self) -> None:
        bundled_bridge = object()
        with patch.object(
            bridge_loader.importlib,
            "import_module",
            return_value=bundled_bridge,
        ) as import_module:
            self.assertIs(
                bridge_loader.import_live_bridge("0.4.5-beta.31"), bundled_bridge
            )
        import_module.assert_called_once_with(
            "endstone_worldgen_studio._endstone_worldgen_live"
        )

    def test_bridge_loader_does_not_mask_bundled_dependency_failure(self) -> None:
        dependency_error = missing_module("native_runtime_dependency")
        with patch.object(
            bridge_loader.importlib,
            "import_module",
            side_effect=dependency_error,
        ) as import_module:
            with self.assertRaises(ModuleNotFoundError) as raised:
                bridge_loader.import_live_bridge("0.4.5-beta.31")
        self.assertIs(raised.exception, dependency_error)
        self.assertEqual(import_module.call_count, 1)

    def test_bridge_loader_reports_required_platform_wheel(self) -> None:
        def import_module(name: str, package: str | None = None):
            del package
            raise missing_module(name)

        with patch.object(
            bridge_loader.importlib,
            "import_module",
            side_effect=import_module,
        ) as imported:
            with self.assertRaisesRegex(
                ModuleNotFoundError,
                "matching 0\\.4\\.5-beta\\.31 CPython 3\\.14 platform wheel",
            ):
                bridge_loader.import_live_bridge("0.4.5-beta.31")
        imported.assert_called_once_with(
            "endstone_worldgen_studio._endstone_worldgen_live"
        )

    def test_every_registered_handler_and_generator_mode_runs(self) -> None:
        plugin, sender = self.make_plugin()
        command = SimpleNamespace(name="wg")

        self.assertTrue(plugin.on_command(sender, command, []))
        self.assertTrue(plugin.on_command(sender, command, ["status"]))
        for generator in ("flat", "island", "maze", "ores"):
            self.assertTrue(
                plugin.on_command(sender, command, ["gen", generator, "1", "-2"])
            )
        for structure in ("castle", "arena"):
            self.assertTrue(
                plugin.on_command(sender, command, ["structure", structure, "1", "-2"])
            )
        self.assertTrue(plugin.on_command(sender, command, ["benchmark", "1"]))
        self.assertTrue(plugin.on_command(sender, command, ["inspect", "1", "-2"]))
        self.assertFalse(plugin.on_command(sender, SimpleNamespace(name="other"), []))
        self.assertTrue(any("Native WorldGen Service Status" in m for m in sender.messages))
        self.assertTrue(any("does not commit chunks" in m for m in sender.messages))
        self.assertFalse(any("Â§" in message for message in sender.messages))

    def test_missing_bridge_stops_reference_generation(self) -> None:
        plugin, sender = self.make_plugin()
        plugin.live_bridge = None
        plugin.bridge_error = "module not found"
        with patch(
            "endstone_worldgen_studio.plugin.import_live_bridge",
            side_effect=ModuleNotFoundError("module not found"),
        ):
            self.assertTrue(
                plugin.on_command(
                    sender, SimpleNamespace(name="wg"), ["gen", "flat", "0", "0"]
                )
            )
        self.assertTrue(any("Native WorldGen service unavailable" in m for m in sender.messages))
        self.assertFalse(any("Generated (0, 0)" in m for m in sender.messages))

    def test_command_retries_bridge_and_creates_scheduler_after_late_service(self) -> None:
        plugin = WorldGenStudioPlugin()
        bridge = FakeLiveBridge()
        bridge.available_now = False
        sender = FakeSender()
        self.addCleanup(plugin.on_disable)

        with patch(
            "endstone_worldgen_studio.plugin.import_live_bridge",
            return_value=bridge,
        ):
            plugin.on_enable()
            self.assertIsNone(plugin.live_bridge)
            self.assertIsNone(plugin.scheduler)
            self.assertEqual(bridge.availability_checks, 1)

            bridge.available_now = True
            self.assertTrue(
                plugin.on_command(sender, SimpleNamespace(name="wg"), ["status"])
            )

        self.assertIs(plugin.live_bridge, bridge)
        self.assertIsNotNone(plugin.scheduler)
        self.assertEqual(bridge.availability_checks, 2)
        self.assertTrue(any("Native WorldGen Service Status" in m for m in sender.messages))


if __name__ == "__main__":
    unittest.main()
