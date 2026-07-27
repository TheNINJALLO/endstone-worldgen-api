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
        self.interception_active_now = True
        self.availability_checks = 0
        self.generate_calls: list[tuple[object, str, int, int, int, str]] = []
        self.live_result = {
            "success": True,
            "committed": True,
            "failure": "none",
            "requested_chunks": 1,
            "planned_blocks": 12,
            "changed_blocks": 12,
            "changed_chunks": 1,
            "unconfirmed_blocks": 0,
            "min_changed_y": 68,
            "max_changed_y": 69,
            "message": "all changed chunks passed commit and flush",
        }

    def available(self, server):
        del server
        self.availability_checks += 1
        return self.available_now

    def status(self, server):
        del server
        return {
            "interception_active": self.interception_active_now,
            "populator_count": 0,
            "stats": {
                "dispatched": 10,
                "committed": 8,
                "waiting": 1,
                "inflight": 1,
                "capture_retries": 2,
                "capture_failures": 3,
                "commit_failures": 4,
                "empty_pipeline_requests": 5,
                "waiting_overflow_drops": 6,
            },
            "live_stats": {
                "requests": 7,
                "successful_requests": 6,
                "failed_requests": 1,
                "changed_blocks": 100,
                "unconfirmed_blocks": 2,
                "descriptor_failures": 8,
                "capture_failures": 9,
                "commit_failures": 10,
                "flush_failures": 11,
                "primary_thread_rejections": 12,
            },
            "diagnostics": {
                "intercepted_requests": 12,
                "dropped_requests": 0,
                "exact_build_match": True,
                "primary_thread": True,
                "hooked_chunk_sources": 3,
                "chunk_source_available": True,
            },
        }

    def generate_live(self, server, dimension, chunk_x, chunk_z, anchor_y, recipe):
        self.generate_calls.append(
            (server, dimension, chunk_x, chunk_z, anchor_y, recipe)
        )
        return dict(self.live_result)


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
            {"gen", "structure", "buffer", "benchmark", "inspect", "status"},
        )
        self.assertEqual(
            command["usages"],
            [
                "/wg",
                (
                    "/wg (gen)<action: WorldGenGenerateAction> "
                    "(flat|island|maze|ores)<generator: WorldGenGenerator>"
                ),
                (
                    "/wg (gen)<action: WorldGenGenerateAtAction> "
                    "(flat|island|maze|ores)<generator: WorldGenGeneratorAt> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                (
                    "/wg (structure)<action: WorldGenStructureAction> "
                    "(castle|arena)<structure: WorldGenStructure>"
                ),
                (
                    "/wg (structure)<action: WorldGenStructureAtAction> "
                    "(castle|arena)<structure: WorldGenStructureAt> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                (
                    "/wg (buffer)<action: WorldGenBufferAction> "
                    "(flat|island|maze|ores)<generator: WorldGenBufferGenerator>"
                ),
                (
                    "/wg (buffer)<action: WorldGenBufferAtAction> "
                    "(flat|island|maze|ores)<generator: WorldGenBufferGeneratorAt> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                "/wg (benchmark)<action: WorldGenBenchmarkAction>",
                (
                    "/wg (benchmark)<action: WorldGenBenchmarkCountAction> "
                    "<chunk_count: int>"
                ),
                "/wg (inspect)<action: WorldGenInspectAction>",
                (
                    "/wg (inspect)<action: WorldGenInspectAtAction> "
                    "<chunk_x: int> <chunk_z: int>"
                ),
                "/wg (status)<action: WorldGenStatusAction>",
            ],
        )
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
        self.addCleanup(plugin.on_disable)
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
            ["WorldGen Studio enabled against the native endstone:worldgen:v2 service."],
        )

    def test_live_service_name_is_abi_versioned(self) -> None:
        service = (ROOT / "include/endstone_worldgen/worldgen_service.h").read_text(
            encoding="utf-8"
        )
        self.assertIn('WorldGenServiceName = "endstone:worldgen:v2"', service)
        self.assertIn("WorldGenServiceAbiVersion = 2", service)

    def test_bridge_loader_prefers_package_relative_companion(self) -> None:
        bundled_bridge = object()
        with patch.object(
            bridge_loader.importlib,
            "import_module",
            return_value=bundled_bridge,
        ) as import_module:
            self.assertIs(
                bridge_loader.import_live_bridge("0.4.5"), bundled_bridge
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
                bridge_loader.import_live_bridge("0.4.5")
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
                "matching 0\\.4\\.5 CPython 3\\.14 platform wheel",
            ):
                bridge_loader.import_live_bridge("0.4.5")
        imported.assert_called_once_with(
            "endstone_worldgen_studio._endstone_worldgen_live"
        )

    def test_every_registered_handler_and_generator_mode_runs(self) -> None:
        plugin, sender = self.make_plugin()
        command = SimpleNamespace(name="wg")

        self.assertTrue(plugin.on_command(sender, command, []))
        self.assertTrue(plugin.on_command(sender, command, ["status"]))
        for generator in ("flat", "island", "maze", "ores"):
            self.assertTrue(plugin.on_command(sender, command, ["gen", generator]))
            self.assertTrue(
                plugin.on_command(sender, command, ["gen", generator, "1", "-2"])
            )
        for structure in ("castle", "arena"):
            self.assertTrue(plugin.on_command(sender, command, ["structure", structure]))
            self.assertTrue(
                plugin.on_command(sender, command, ["structure", structure, "1", "-2"])
            )
        for generator in ("flat", "island", "maze", "ores"):
            self.assertTrue(plugin.on_command(sender, command, ["buffer", generator]))
            self.assertTrue(
                plugin.on_command(sender, command, ["buffer", generator, "1", "-2"])
            )
        self.assertTrue(plugin.on_command(sender, command, ["benchmark"]))
        self.assertTrue(plugin.on_command(sender, command, ["benchmark", "1"]))
        self.assertTrue(plugin.on_command(sender, command, ["inspect"]))
        self.assertTrue(plugin.on_command(sender, command, ["inspect", "1", "-2"]))
        self.assertFalse(plugin.on_command(sender, SimpleNamespace(name="other"), []))
        self.assertTrue(any("Native WorldGen Service Status" in m for m in sender.messages))
        self.assertTrue(any("does not capture, change, or commit" in m for m in sender.messages))
        self.assertTrue(any("committed and flushed successfully" in m for m in sender.messages))
        self.assertTrue(any("may write underground" in m for m in sender.messages))
        self.assertFalse(any("Â§" in message for message in sender.messages))

    def test_missing_bridge_stops_live_generation_but_not_detached_buffer(self) -> None:
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
        self.assertFalse(any("committed and flushed successfully" in m for m in sender.messages))
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["buffer", "flat", "0", "0"]
            )
        )
        self.assertTrue(any("Detached buffer generated" in m for m in sender.messages))

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
            self.assertIsNotNone(plugin.scheduler)
            self.assertEqual(bridge.availability_checks, 1)

            bridge.available_now = True
            self.assertTrue(
                plugin.on_command(sender, SimpleNamespace(name="wg"), ["status"])
            )

        self.assertIs(plugin.live_bridge, bridge)
        self.assertIsNotNone(plugin.scheduler)
        self.assertEqual(bridge.availability_checks, 2)
        self.assertTrue(any("Native WorldGen Service Status" in m for m in sender.messages))

    def test_live_command_calls_native_bridge_and_waits_for_flush_confirmation(self) -> None:
        plugin, sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)

        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["gen", "flat", "4", "-7"]
            )
        )
        self.assertEqual(
            bridge.generate_calls,
            [(plugin.server, "overworld", 4, -7, 69, "flat")],
        )
        self.assertTrue(any("bounded recipe band Y=68..69" in m for m in sender.messages))
        self.assertTrue(any("committed and flushed successfully" in m for m in sender.messages))
        self.assertTrue(any("actual changed range Y=68..69" in m for m in sender.messages))

        sender.messages.clear()
        bridge.live_result = {
            "success": False,
            "committed": False,
            "failure": "flush",
            "planned_blocks": 12,
            "changed_blocks": 0,
            "changed_chunks": 0,
            "unconfirmed_blocks": 12,
            "min_changed_y": 68,
            "max_changed_y": 69,
            "message": "commit ran but flush failed",
        }
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["gen", "flat", "4", "-7"]
            )
        )
        self.assertTrue(any("failed [flush]" in m for m in sender.messages))
        self.assertTrue(any("native write range Y=68..69" in m for m in sender.messages))
        self.assertTrue(any("writes not confirmed by flush: 12" in m for m in sender.messages))
        self.assertFalse(any("committed and flushed successfully" in m for m in sender.messages))

        sender.messages.clear()
        bridge.live_result = {
            "success": True,
            "committed": False,
            "failure": "none",
            "planned_blocks": 0,
            "changed_blocks": 0,
            "changed_chunks": 0,
            "unconfirmed_blocks": 0,
            "min_changed_y": None,
            "max_changed_y": None,
            "message": "the live recipe is already present",
        }
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["gen", "flat", "4", "-7"]
            )
        )
        self.assertTrue(any("completed with 0 changed blocks" in m for m in sender.messages))
        self.assertTrue(any("no native write range" in m for m in sender.messages))

    def test_negative_fractional_player_coordinates_use_floor_division(self) -> None:
        plugin, sender = self.make_plugin()
        sender.location.x = -0.5
        sender.location.z = -16.2
        self.assertTrue(
            plugin.on_command(sender, SimpleNamespace(name="wg"), ["gen", "flat"])
        )
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        self.assertEqual(bridge.generate_calls[-1][2:4], (-1, -2))

    def test_player_y_is_floored_then_shifted_to_the_block_below(self) -> None:
        plugin, sender = self.make_plugin()
        sender.location.y = 100.9
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["structure", "arena", "2", "3"]
            )
        )
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        self.assertEqual(bridge.generate_calls[-1][4], 99)
        self.assertTrue(any("anchor surface/floor Y=99" in m for m in sender.messages))

    def test_manual_live_recipe_is_not_gated_on_chunk_interception(self) -> None:
        plugin, sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        bridge.interception_active_now = False
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["gen", "flat", "2", "3"]
            )
        )
        self.assertEqual(bridge.generate_calls[-1][2:4], (2, 3))
        self.assertTrue(any("committed and flushed successfully" in m for m in sender.messages))

        sender.messages.clear()
        self.assertTrue(plugin.on_command(sender, SimpleNamespace(name="wg"), ["status"]))
        self.assertTrue(any("interception is inactive" in m for m in sender.messages))
        self.assertTrue(any("Manual live recipes remain available" in m for m in sender.messages))

    def test_live_target_validation_fails_closed(self) -> None:
        plugin, sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        command = SimpleNamespace(name="wg")

        self.assertTrue(plugin.on_command(sender, command, ["gen", "flat", "1"]))
        self.assertTrue(plugin.on_command(sender, command, ["gen", "flat", "bad", "2"]))
        self.assertTrue(
            plugin.on_command(sender, command, ["gen", "flat", "134217728", "0"])
        )
        sender.location.y = float("nan")
        self.assertTrue(plugin.on_command(sender, command, ["gen", "flat", "0", "0"]))
        console = FakeSender()
        console.location = None
        self.assertTrue(plugin.on_command(console, command, ["gen", "flat", "0", "0"]))
        self.assertEqual(bridge.generate_calls, [])
        self.assertTrue(any("Chunk coordinates must be integers" in m for m in sender.messages))
        self.assertTrue(any("safe world range" in m for m in sender.messages))
        self.assertTrue(any("cannot be converted to a live recipe anchor" in m for m in sender.messages))
        self.assertTrue(any("console defaults are disabled" in m for m in console.messages))

    def test_live_result_y_range_validation_fails_closed(self) -> None:
        plugin, sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        bridge.live_result["min_changed_y"] = None
        bridge.live_result["max_changed_y"] = None
        self.assertTrue(
            plugin.on_command(
                sender, SimpleNamespace(name="wg"), ["gen", "flat", "0", "0"]
            )
        )
        self.assertTrue(any("without a changed-Y range" in m for m in sender.messages))
        self.assertFalse(any("committed and flushed successfully" in m for m in sender.messages))

    def test_status_surfaces_zero_pipeline_and_failure_counters(self) -> None:
        plugin, sender = self.make_plugin()
        self.assertTrue(plugin.on_command(sender, SimpleNamespace(name="wg"), ["status"]))
        self.assertTrue(any("0 native populators" in m for m in sender.messages))
        self.assertTrue(any("Capture retries/failures" in m and "2/3" in m for m in sender.messages))
        self.assertTrue(any("Commit failures" in m and "4" in m for m in sender.messages))
        self.assertTrue(any("Empty pipeline" in m and "5" in m for m in sender.messages))
        self.assertTrue(any("Manual live requests/success/failure" in m and "7/6/1" in m for m in sender.messages))
        self.assertTrue(any("Manual descriptor/capture/commit/flush/thread failures" in m and "8/9/10/11/12" in m for m in sender.messages))

    def test_benchmark_rejects_out_of_range_counts_instead_of_clamping(self) -> None:
        plugin, sender = self.make_plugin()
        command = SimpleNamespace(name="wg")
        self.assertTrue(plugin.on_command(sender, command, ["benchmark", "0"]))
        self.assertTrue(plugin.on_command(sender, command, ["benchmark", "129"]))
        self.assertEqual(
            sum("Chunk count must be an integer from 1 to 128" in m for m in sender.messages),
            2,
        )
        self.assertFalse(any("Generating 1 detached" in m for m in sender.messages))


if __name__ == "__main__":
    unittest.main()
