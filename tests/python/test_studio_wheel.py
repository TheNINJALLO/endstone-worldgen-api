from __future__ import annotations

import ast
import inspect
import json
import logging
from pathlib import Path
from types import ModuleType, SimpleNamespace
import sys
import tomllib
import unittest
from unittest.mock import patch
from uuid import UUID, uuid4


ROOT = Path(__file__).resolve().parents[2]
PLUGIN_PROJECT = ROOT / "examples" / "python" / "world_gen_studio_plugin"
PLUGIN_SOURCE = PLUGIN_PROJECT / "src"


def install_endstone_test_double() -> None:
    endstone_module = ModuleType("endstone")
    command_module = ModuleType("endstone.command")
    event_module = ModuleType("endstone.event")
    form_module = ModuleType("endstone.form")
    plugin_module = ModuleType("endstone.plugin")

    class Plugin:
        def __init__(self) -> None:
            self.server = object()
            self.logger = logging.getLogger(type(self).__name__)
            self.registered_listeners: list[object] = []

        def register_events(self, listener: object) -> None:
            self.registered_listeners.append(listener)

    class Command:
        def __init__(self, name: str):
            self.name = name

    class CommandSender:
        pass

    class Event:
        pass

    class PlayerQuitEvent(Event):
        def __init__(self, player) -> None:
            self.player = player

    class PlayerDeathEvent(Event):
        def __init__(self, player) -> None:
            self.player = player

    def event_handler(func):
        func._is_event_handler = True
        func._priority = 0
        func._ignore_cancelled = False
        return func

    class Button:
        def __init__(self, text="", icon=None, on_click=None) -> None:
            self.text = text
            self.icon = icon
            self.on_click = on_click

    class Divider:
        pass

    class Header:
        def __init__(self, label="") -> None:
            self.label = label

    class Label:
        def __init__(self, text="") -> None:
            self.text = text

    class Dropdown:
        def __init__(self, label="", options=None, default_index=None) -> None:
            self.label = label
            self.options = list(options or [])
            self.default_index = default_index

    class Slider:
        def __init__(
            self, label="", min=0, max=100, step=20, default_value=None
        ) -> None:
            self.label = label
            self.min = min
            self.max = max
            self.step = step
            self.default_value = default_value

    class StepSlider(Dropdown):
        pass

    class TextInput:
        def __init__(self, label="", placeholder="", default_value=None) -> None:
            self.label = label
            self.placeholder = placeholder
            self.default_value = default_value

    class Toggle:
        def __init__(self, label="", default_value=False) -> None:
            self.label = label
            self.default_value = default_value

    class ActionForm:
        def __init__(
            self,
            title="",
            content="",
            buttons=None,
            on_submit=None,
            on_close=None,
        ) -> None:
            self.title = title
            self.content = content
            self._controls = list(buttons or [])
            self.on_submit = on_submit
            self.on_close = on_close

        @property
        def controls(self):
            return list(self._controls)

        @controls.setter
        def controls(self, value) -> None:
            self._controls = list(value)

        def add_button(self, text, icon=None, on_click=None):
            self._controls.append(Button(text, icon, on_click))
            return self

        def add_label(self, text):
            self._controls.append(Label(text))
            return self

        def add_header(self, text):
            self._controls.append(Header(text))
            return self

        def add_divider(self):
            self._controls.append(Divider())
            return self

    class ModalForm:
        def __init__(
            self,
            title="",
            controls=None,
            submit_button=None,
            icon=None,
            on_submit=None,
            on_close=None,
        ) -> None:
            self.title = title
            self._controls = list(controls or [])
            self.submit_button = submit_button
            self.icon = icon
            self.on_submit = on_submit
            self.on_close = on_close

        @property
        def controls(self):
            return list(self._controls)

        @controls.setter
        def controls(self, value) -> None:
            self._controls = list(value)

        def add_control(self, control):
            self._controls.append(control)
            return self

    plugin_module.Plugin = Plugin
    command_module.Command = Command
    command_module.CommandSender = CommandSender
    event_module.Event = Event
    event_module.PlayerQuitEvent = PlayerQuitEvent
    event_module.PlayerDeathEvent = PlayerDeathEvent
    event_module.event_handler = event_handler
    for form_class in (
        ActionForm,
        Button,
        Divider,
        Dropdown,
        Header,
        Label,
        ModalForm,
        Slider,
        StepSlider,
        TextInput,
        Toggle,
    ):
        setattr(form_module, form_class.__name__, form_class)
    endstone_module.plugin = plugin_module
    endstone_module.command = command_module
    endstone_module.event = event_module
    endstone_module.form = form_module
    sys.modules["endstone"] = endstone_module
    sys.modules["endstone.plugin"] = plugin_module
    sys.modules["endstone.command"] = command_module
    sys.modules["endstone.event"] = event_module
    sys.modules["endstone.form"] = form_module


install_endstone_test_double()
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(PLUGIN_SOURCE))

from endstone_worldgen_studio import WorldGenStudioPlugin  # noqa: E402
from endstone_worldgen_studio import _bridge_loader as bridge_loader  # noqa: E402
from endstone_worldgen import GenerationScheduler  # noqa: E402
from endstone.event import Event, PlayerDeathEvent, PlayerQuitEvent  # noqa: E402
from endstone.form import (  # noqa: E402
    ActionForm,
    Dropdown,
    ModalForm,
    TextInput,
    Toggle,
)


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


class FakePlayerSender(FakeSender):
    def __init__(self, unique_id: UUID | None = None) -> None:
        super().__init__()
        self.unique_id = unique_id or uuid4()
        self.is_valid = True
        self.is_dead = False
        self.permission_granted = True
        self.permission_checks: list[str] = []
        self.sent_forms: list[object] = []
        self.send_form_error: Exception | None = None

    def has_permission(self, permission: str) -> bool:
        self.permission_checks.append(permission)
        return self.permission_granted

    def send_form(self, form) -> None:
        if self.send_form_error is not None:
            raise self.send_form_error
        self.sent_forms.append(form)


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
            {"menu", "gen", "structure", "buffer", "benchmark", "inspect", "status"},
        )
        self.assertEqual(
            command["usages"],
            [
                "/wg",
                "/wg (menu)<action: WorldGenMenuAction>",
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

    def test_event_handlers_have_runtime_types_and_are_registered(self) -> None:
        for handler_name in ("on_player_quit", "on_player_death"):
            handler = getattr(WorldGenStudioPlugin, handler_name)
            annotation = inspect.signature(handler).parameters["event"].annotation
            self.assertTrue(inspect.isclass(annotation))
            self.assertTrue(issubclass(annotation, Event))
            self.assertTrue(getattr(handler, "_is_event_handler", False))

        plugin = WorldGenStudioPlugin()
        with patch(
            "endstone_worldgen_studio.plugin.import_live_bridge",
            return_value=FakeLiveBridge(),
        ):
            plugin.on_enable()
        self.addCleanup(plugin.on_disable)
        self.assertEqual(plugin.registered_listeners, [plugin])

    def test_player_menu_entrypoints_lock_once_and_console_keeps_help(self) -> None:
        plugin, _sender = self.make_plugin()
        command = SimpleNamespace(name="wg")
        console = FakeSender()

        self.assertTrue(plugin.on_command(console, command, []))
        self.assertTrue(any("WorldGen Studio Test Plugin" in m for m in console.messages))
        self.assertTrue(plugin.on_command(console, command, ["menu"]))
        self.assertTrue(any("only available to players" in m for m in console.messages))

        player = FakePlayerSender()
        self.assertTrue(plugin.on_command(player, command, []))
        self.assertEqual(len(player.sent_forms), 1)
        root_form = player.sent_forms[-1]
        self.assertIsInstance(root_form, ActionForm)
        self.assertEqual(len(root_form.controls), 6)
        self.assertTrue(all(button.on_click is None for button in root_form.controls))
        self.assertIn("write to the current world", root_form.content)
        self.assertIn("never edit the live world", root_form.content)
        self.assertIn("World Write", root_form.controls[0].text)
        self.assertIn("No World Changes", root_form.controls[2].text)
        self.assertIn(player.unique_id, plugin.active_forms)

        self.assertTrue(plugin.on_command(player, command, ["menu"]))
        self.assertEqual(len(player.sent_forms), 1)
        self.assertTrue(any("already open" in m for m in player.messages))

        root_form.on_close(player)
        self.assertNotIn(player.unique_id, plugin.active_forms)
        self.assertTrue(plugin.on_command(player, command, ["menu"]))
        self.assertEqual(len(player.sent_forms), 2)
        player.sent_forms[-1].on_close(player)

        self.assertTrue(plugin.on_command(player, command, ["menu", "extra"]))
        self.assertTrue(any("Usage: /wg menu" in m for m in player.messages))

    def test_main_menu_routes_every_command_group(self) -> None:
        plugin, _sender = self.make_plugin()
        routes = (
            (0, "_open_recipe_form", "gen"),
            (1, "_open_recipe_form", "structure"),
            (2, "_open_recipe_form", "buffer"),
            (3, "_open_benchmark_form", None),
            (4, "_open_inspect_form", None),
            (5, "_handle_status", []),
        )
        for selection, method_name, extra in routes:
            with self.subTest(selection=selection):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_main_menu(player))
                root_form = player.sent_forms[-1]
                with patch.object(plugin, method_name) as routed:
                    root_form.on_submit(player, selection)
                if method_name == "_open_recipe_form":
                    routed.assert_called_once_with(player, extra)
                elif method_name == "_handle_status":
                    routed.assert_called_once_with(player, [])
                else:
                    routed.assert_called_once_with(player)
                self.assertNotIn(player.unique_id, plugin.active_forms)

        player = FakePlayerSender()
        self.assertTrue(plugin._open_main_menu(player))
        invalid_form = player.sent_forms[-1]
        invalid_form.on_submit(player, True)
        self.assertEqual(len(player.sent_forms), 2)
        self.assertTrue(any("invalid button selection" in m for m in player.messages))

    def test_live_menu_requires_confirmation_and_dispatches_every_recipe_once(self) -> None:
        plugin, _sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)

        for command_name, recipes in (
            ("gen", plugin._GENERATOR_RECIPES),
            ("structure", plugin._STRUCTURE_RECIPES),
        ):
            for recipe_index, recipe in enumerate(recipes):
                with self.subTest(command=command_name, recipe=recipe):
                    player = FakePlayerSender()
                    self.assertTrue(plugin._open_recipe_form(player, command_name))
                    modal = player.sent_forms[-1]
                    self.assertIsInstance(modal, ModalForm)
                    self.assertEqual(
                        [type(control) for control in modal.controls],
                        [Dropdown, Toggle, TextInput, TextInput],
                    )
                    handler_name = plugin._SUBCOMMAND_HANDLERS[command_name]
                    original_handler = getattr(plugin, handler_name)
                    before_writes = len(bridge.generate_calls)
                    with patch.object(
                        plugin, handler_name, wraps=original_handler
                    ) as handler:
                        modal.on_submit(
                            player,
                            json.dumps([recipe_index, False, "4", "-7"]),
                        )
                        handler.assert_not_called()
                        self.assertEqual(len(bridge.generate_calls), before_writes)
                        confirmation = player.sent_forms[-1]
                        self.assertIsInstance(confirmation, ActionForm)
                        self.assertIn("Confirm Live World Write", confirmation.title)
                        self.assertTrue(
                            all(button.on_click is None for button in confirmation.controls)
                        )

                        # A duplicate response from the replaced modal is stale and inert.
                        modal.on_submit(
                            player,
                            json.dumps([recipe_index, False, "4", "-7"]),
                        )
                        self.assertIs(player.sent_forms[-1], confirmation)
                        confirmation.on_submit(player, 0)
                        handler.assert_called_once_with(player, [recipe, "4", "-7"])
                        self.assertEqual(len(bridge.generate_calls), before_writes + 1)
                        self.assertEqual(
                            bridge.generate_calls[-1][1:],
                            ("overworld", 4, -7, 69, recipe),
                        )

                        # A duplicate confirmation cannot repeat a live write.
                        confirmation.on_submit(player, 0)
                        self.assertEqual(len(bridge.generate_calls), before_writes + 1)

        player = FakePlayerSender()
        self.assertTrue(plugin._open_recipe_form(player, "gen"))
        modal = player.sent_forms[-1]
        original_handler = plugin._handle_gen
        with patch.object(plugin, "_handle_gen", wraps=original_handler) as handler:
            modal.on_submit(player, json.dumps([0, True, "ignored", "ignored"]))
            player.sent_forms[-1].on_submit(player, 0)
        handler.assert_called_once_with(player, ["flat"])
        self.assertEqual(bridge.generate_calls[-1][2:4], (2, -1))

    def test_live_menu_defaults_to_a_visible_recipe_and_labels_hidden_work(self) -> None:
        plugin, _sender = self.make_plugin()

        live_player = FakePlayerSender()
        self.assertTrue(plugin._open_recipe_form(live_player, "gen"))
        live_dropdown = live_player.sent_forms[-1].controls[0]
        self.assertEqual(
            plugin._GENERATOR_RECIPES[live_dropdown.default_index], "maze"
        )
        self.assertIn("Clearly Visible", live_dropdown.options[live_dropdown.default_index])
        self.assertTrue(any("Underground" in option for option in live_dropdown.options))
        live_player.sent_forms[-1].on_close(live_player)

        detached_player = FakePlayerSender()
        self.assertTrue(plugin._open_recipe_form(detached_player, "buffer"))
        detached_dropdown = detached_player.sent_forms[-1].controls[0]
        self.assertTrue(
            all("Detached Only" in option for option in detached_dropdown.options)
        )
        detached_player.sent_forms[-1].on_close(detached_player)

    def test_live_confirmation_back_and_cancel_never_write(self) -> None:
        plugin, _sender = self.make_plugin()
        bridge = plugin.live_bridge
        self.assertIsInstance(bridge, FakeLiveBridge)
        player = FakePlayerSender()

        self.assertTrue(plugin._open_recipe_form(player, "gen"))
        recipe_form = player.sent_forms[-1]
        recipe_form.on_submit(player, json.dumps([0, False, "1", "2"]))
        confirmation = player.sent_forms[-1]
        confirmation.on_submit(player, 1)
        self.assertIsInstance(player.sent_forms[-1], ModalForm)
        self.assertEqual(bridge.generate_calls, [])

        # Cancelling a child form navigates back without invoking a command.
        player.sent_forms[-1].on_close(player)
        self.assertIsInstance(player.sent_forms[-1], ActionForm)
        self.assertEqual(len(player.sent_forms[-1].controls), 6)
        self.assertEqual(bridge.generate_calls, [])
        player.sent_forms[-1].on_close(player)

        self.assertTrue(plugin._open_recipe_form(player, "structure"))
        player.sent_forms[-1].on_submit(player, json.dumps([1, True, "", ""]))
        confirmation = player.sent_forms[-1]
        confirmation.on_close(player)
        self.assertIsInstance(player.sent_forms[-1], ModalForm)
        self.assertEqual(bridge.generate_calls, [])

    def test_detached_menu_forms_delegate_exact_handler_args(self) -> None:
        plugin, _sender = self.make_plugin()

        for recipe_index, recipe in enumerate(plugin._GENERATOR_RECIPES):
            with self.subTest(buffer=recipe):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_recipe_form(player, "buffer"))
                with patch.object(plugin, "_handle_buffer") as handler:
                    player.sent_forms[-1].on_submit(
                        player,
                        json.dumps([recipe_index, False, "-8", "9"]),
                    )
                handler.assert_called_once_with(player, [recipe, "-8", "9"])

        for response, expected in (
            ([True, "not-used"], []),
            ([False, "1"], ["1"]),
            ([False, "128"], ["128"]),
        ):
            with self.subTest(benchmark=response):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_benchmark_form(player))
                with patch.object(plugin, "_handle_benchmark") as handler:
                    player.sent_forms[-1].on_submit(player, json.dumps(response))
                handler.assert_called_once_with(player, expected)

        for response, expected in (
            ([True, "", ""], []),
            (
                [False, "-134217728", "134217727"],
                ["-134217728", "134217727"],
            ),
        ):
            with self.subTest(inspect=response):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_inspect_form(player))
                with patch.object(plugin, "_handle_inspect") as handler:
                    player.sent_forms[-1].on_submit(player, json.dumps(response))
                handler.assert_called_once_with(player, expected)

    def test_modal_responses_are_strictly_validated_and_fail_closed(self) -> None:
        plugin, _sender = self.make_plugin()
        invalid_responses = (
            [0, True, "", ""],
            "{",
            "{}",
            json.dumps([0, True, ""]),
            json.dumps([True, True, "", ""]),
            json.dumps([4, True, "", ""]),
            json.dumps([0, 1, "", ""]),
            json.dumps([0, False, 1, "2"]),
            json.dumps([0, False, "", "2"]),
            json.dumps([0, False, "bad", "2"]),
            json.dumps([0, False, "134217728", "0"]),
        )
        for response in invalid_responses:
            with self.subTest(response=response):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_recipe_form(player, "buffer"))
                with patch.object(plugin, "_handle_buffer") as handler:
                    player.sent_forms[-1].on_submit(player, response)
                handler.assert_not_called()
                self.assertEqual(len(player.sent_forms), 2)
                self.assertIn(player.unique_id, plugin.active_forms)
                player.sent_forms[-1].on_close(player)

        for response in (
            json.dumps([False, 1]),
            json.dumps([False, "0"]),
            json.dumps([False, "129"]),
        ):
            with self.subTest(benchmark=response):
                player = FakePlayerSender()
                self.assertTrue(plugin._open_benchmark_form(player))
                with patch.object(plugin, "_handle_benchmark") as handler:
                    player.sent_forms[-1].on_submit(player, response)
                handler.assert_not_called()
                self.assertEqual(len(player.sent_forms), 2)

    def test_uuid_lock_permission_recheck_and_send_failure_cleanup(self) -> None:
        plugin, _sender = self.make_plugin()
        shared_id = uuid4()
        first = FakePlayerSender(shared_id)
        same_player = FakePlayerSender(shared_id)
        other = FakePlayerSender()

        self.assertTrue(plugin._open_main_menu(first))
        first_form = first.sent_forms[-1]
        first_token = plugin.active_forms[shared_id]
        self.assertFalse(plugin._open_main_menu(same_player))
        self.assertEqual(same_player.sent_forms, [])
        self.assertTrue(plugin._open_main_menu(other))

        # A mismatched callback sender cannot release or act under another UUID.
        first_form.on_submit(other, 0)
        self.assertEqual(plugin.active_forms[shared_id], first_token)
        self.assertIn(other.unique_id, plugin.active_forms)

        first_form.on_close(first)
        self.assertNotIn(shared_id, plugin.active_forms)
        other.sent_forms[-1].on_close(other)

        failing = FakePlayerSender()
        failing.send_form_error = RuntimeError("send failed")
        self.assertFalse(plugin._open_main_menu(failing))
        self.assertNotIn(failing.unique_id, plugin.active_forms)
        self.assertTrue(any("send failed" in message for message in failing.messages))

        # A stale callback cannot clear a newer token or navigate.
        self.assertTrue(plugin._open_main_menu(first))
        stale_form = first.sent_forms[-1]
        stale_form.on_close(first)
        self.assertTrue(plugin._open_main_menu(first))
        current_token = plugin.active_forms[shared_id]
        sent_count = len(first.sent_forms)
        stale_form.on_submit(first, 0)
        self.assertEqual(plugin.active_forms[shared_id], current_token)
        self.assertEqual(len(first.sent_forms), sent_count)
        first.sent_forms[-1].on_close(first)

        revoked = FakePlayerSender()
        self.assertTrue(plugin._open_main_menu(revoked))
        revoked.permission_granted = False
        revoked.sent_forms[-1].on_submit(revoked, 0)
        self.assertNotIn(revoked.unique_id, plugin.active_forms)
        self.assertEqual(len(revoked.sent_forms), 1)
        self.assertTrue(any("no longer active" in m for m in revoked.messages))

        unavailable = FakePlayerSender()
        self.assertTrue(plugin._open_main_menu(unavailable))
        unavailable.is_dead = True
        unavailable.sent_forms[-1].on_submit(unavailable, 0)
        self.assertNotIn(unavailable.unique_id, plugin.active_forms)
        self.assertEqual(len(unavailable.sent_forms), 1)

    def test_form_locks_cleanup_on_quit_death_and_disable(self) -> None:
        plugin, _sender = self.make_plugin()
        quitting = FakePlayerSender()
        dying = FakePlayerSender()
        self.assertTrue(plugin._open_main_menu(quitting))
        self.assertTrue(plugin._open_main_menu(dying))
        stale_quit_form = quitting.sent_forms[-1]

        plugin.on_player_quit(PlayerQuitEvent(quitting))
        self.assertNotIn(quitting.unique_id, plugin.active_forms)
        self.assertIn(dying.unique_id, plugin.active_forms)
        plugin.on_player_death(PlayerDeathEvent(dying))
        self.assertEqual(plugin.active_forms, {})

        stale_quit_form.on_submit(quitting, 0)
        self.assertEqual(len(quitting.sent_forms), 1)

        self.assertTrue(plugin._open_main_menu(quitting))
        stale_disable_form = quitting.sent_forms[-1]
        plugin.on_disable()
        self.assertEqual(plugin.active_forms, {})
        self.assertIsNone(plugin.scheduler)
        stale_disable_form.on_submit(quitting, 0)
        self.assertEqual(len(quitting.sent_forms), 2)

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
            [
                "WorldGen Studio enabled against the native endstone:worldgen:v2 service; "
                "live /wg gen and /wg structure writes are available, while detached "
                "commands do not edit the world."
            ],
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
                bridge_loader.import_live_bridge("0.4.7"), bundled_bridge
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
                bridge_loader.import_live_bridge("0.4.7")
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
                "matching 0\\.4\\.7 CPython 3\\.14 platform wheel",
            ):
                bridge_loader.import_live_bridge("0.4.7")
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
        self.assertTrue(any("Automatic interception is idle" in m for m in sender.messages))
        self.assertTrue(any("does not affect /wg gen or /wg structure" in m for m in sender.messages))
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
