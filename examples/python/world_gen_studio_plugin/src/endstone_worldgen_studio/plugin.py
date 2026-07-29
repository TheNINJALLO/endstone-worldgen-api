"""Endstone WorldGen live-recipe and detached-buffer command test plugin."""

import json
import math
import random
import time
from typing import Any, Callable
from uuid import UUID, uuid4

from endstone.command import Command, CommandSender
from endstone.event import PlayerDeathEvent, PlayerQuitEvent, event_handler
from endstone.form import ActionForm, Dropdown, ModalForm, TextInput, Toggle
from endstone.plugin import Plugin

from endstone_worldgen import (
    ChunkBuffer,
    ChunkPos,
    FlatGenerator,
    GenerationContext,
    GenerationScheduler,
)

from ._bridge_loader import import_live_bridge


class CustomIslandGenerator:
    identifier = "endstone:floating_island"

    def __init__(self, core_y: int = 100, radius: int = 7):
        self.core_y = core_y
        self.radius = radius

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        buffer.set_biome(8, self.core_y, 8, 9)
        for x in range(16):
            for z in range(16):
                distance = ((x - 8) ** 2 + (z - 8) ** 2) ** 0.5
                if distance > self.radius:
                    continue
                height = int(self.radius - distance + 3)
                for offset in range(-height, height):
                    y = self.core_y + offset
                    if offset == height - 1:
                        block = 2  # grass
                    elif offset > 0:
                        block = 3  # dirt
                    elif offset == 0:
                        block = 56  # diamond ore core
                    else:
                        block = 1  # stone
                    buffer.set(x, y, z, block)


class CustomMazeGenerator:
    identifier = "endstone:dungeon_maze"

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        del context
        buffer.fill(0, 64, 0, 15, 64, 15, 98)
        buffer.fill(0, 69, 0, 15, 69, 15, 98)
        for x in range(16):
            for z in range(16):
                if x in (0, 15) or z in (0, 15) or (x % 4 == 0 and z % 4 != 2):
                    buffer.fill(x, 65, z, x, 68, z, 98)


class CustomOreGenerator:
    identifier = "endstone:ore_test"

    def generate(self, context: GenerationContext, buffer: ChunkBuffer) -> None:
        FlatGenerator(surface_y=64, base=1, top=2).generate(context, buffer)
        rng = random.Random(context.stage_seed)
        for _ in range(32):
            buffer.set(rng.randrange(16), rng.randrange(5, 49), rng.randrange(16), 56)


class WorldGenStudioPlugin(Plugin):
    """Exercise live native recipes and detached scheduler buffers from commands."""

    api_version = "0.11"
    version = "0.4.7"
    description = "Interactive live WorldGen and detached-buffer test suite"
    depend = ["worldgen_api"]

    commands = {
        "wg": {
            "description": "WorldGen live recipe and detached-buffer test suite",
            "usages": [
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
            "permissions": ["wg.admin"],
        }
    }

    permissions = {
        "wg.admin": {
            "description": "Allows access to WorldGen Studio commands",
            "default": "op",
        }
    }

    _SUBCOMMAND_HANDLERS = {
        "menu": "_handle_menu",
        "gen": "_handle_gen",
        "structure": "_handle_structure",
        "buffer": "_handle_buffer",
        "benchmark": "_handle_benchmark",
        "inspect": "_handle_inspect",
        "status": "_handle_status",
    }

    _FORM_PERMISSION = "wg.admin"
    _GENERATOR_RECIPES = ("flat", "island", "maze", "ores")
    _STRUCTURE_RECIPES = ("castle", "arena")
    _MIN_CHUNK_COORDINATE = -134_217_728
    _MAX_CHUNK_COORDINATE = 134_217_727

    def __init__(self) -> None:
        super().__init__()
        self.active_forms: dict[UUID, UUID] = {}

    def on_enable(self) -> None:
        self.active_forms.clear()
        self.scheduler = GenerationScheduler(workers=4)
        self.live_bridge = None
        self.bridge_error = "native bridge was not initialized"
        self.register_events(self)
        self._connect_bridge()

        if self.live_bridge is None:
            self.logger.error(
                "WorldGen live bridge unavailable; live /wg commands will report unavailable "
                "while detached buffer commands remain usable: "
                f"{self.bridge_error}"
            )
        else:
            self.logger.info(
                "WorldGen Studio enabled against the native endstone:worldgen:v2 service; "
                "live /wg gen and /wg structure writes are available, while detached commands do not edit the world."
            )

    def _connect_bridge(self) -> Any | None:
        """Connect to the native service, allowing command-time recovery."""
        try:
            bridge = import_live_bridge(self.version)
            if not bridge.available(self.server):
                self.live_bridge = None
                self.bridge_error = "endstone:worldgen:v2 native service is not registered"
                return None
        except Exception as error:
            self.live_bridge = None
            self.bridge_error = str(error)
            return None

        self.live_bridge = bridge
        self.bridge_error = ""
        return bridge

    def on_disable(self) -> None:
        self.active_forms.clear()
        scheduler = getattr(self, "scheduler", None)
        if scheduler is not None:
            scheduler.close()
            self.scheduler = None

    @event_handler
    def on_player_quit(self, event: PlayerQuitEvent) -> None:
        self._clear_form_lock(event.player)

    @event_handler
    def on_player_death(self, event: PlayerDeathEvent) -> None:
        self._clear_form_lock(event.player)

    def on_command(
        self, sender: CommandSender, command: Command, args: list[str]
    ) -> bool:
        if command.name != "wg":
            return False

        if not args:
            if self._supports_forms(sender):
                self._open_main_menu(sender)
            else:
                self._send_help(sender)
            return True

        handler_name = self._SUBCOMMAND_HANDLERS.get(args[0].lower())
        if handler_name is None:
            self._send_help(sender)
            return True
        return getattr(self, handler_name)(sender, args[1:])

    def _handle_menu(self, sender: CommandSender, args: list[str]) -> bool:
        if args:
            sender.send_message("§cUsage: /wg menu")
            return True
        if not self._supports_forms(sender):
            sender.send_message("§cThe WorldGen menu is only available to players.")
            self._send_help(sender)
            return True
        self._open_main_menu(sender)
        return True

    def _send_help(self, sender: CommandSender) -> None:
        sender.send_message(f"§e=== WorldGen Studio Test Plugin (v{self.version}) ===")
        sender.send_message("§a/wg menu                             §7- Open the in-game WorldGen menu")
        sender.send_message(
            "§a/wg gen <flat|island|maze|ores> [cx cz] §7- Commit one bounded live recipe"
        )
        sender.send_message(
            "§a/wg structure <castle|arena> [cx cz] §7- Commit a bounded 3x3 live recipe"
        )
        sender.send_message(
            "§a/wg buffer <flat|island|maze|ores> [cx cz] §7- Generate a detached reference buffer"
        )
        sender.send_message(
            "§a/wg benchmark [chunk_count]          §7- Run a detached-buffer stress test"
        )
        sender.send_message(
            "§a/wg inspect [cx cz]                  §7- Inspect a detached reference buffer"
        )
        sender.send_message(
            "§a/wg status                           §7- Inspect native interceptor status"
        )

    @staticmethod
    def _supports_forms(sender: CommandSender) -> bool:
        return callable(getattr(sender, "send_form", None))

    @staticmethod
    def _form_player_key(sender: CommandSender) -> UUID | None:
        unique_id = getattr(sender, "unique_id", None)
        return unique_id if isinstance(unique_id, UUID) else None

    def _has_form_permission(
        self, sender: CommandSender, *, notify: bool = True
    ) -> bool:
        checker = getattr(sender, "has_permission", None)
        try:
            allowed = callable(checker) and bool(checker(self._FORM_PERMISSION))
        except Exception:
            allowed = False
        if not allowed and notify:
            sender.send_message(
                "§cYour wg.admin permission is no longer active; the menu action was cancelled."
            )
        return allowed

    @staticmethod
    def _form_player_available(
        sender: CommandSender, *, notify: bool = True
    ) -> bool:
        try:
            available = bool(getattr(sender, "is_valid", True)) and not bool(
                getattr(sender, "is_dead", False)
            )
        except Exception:
            available = False
        if not available and notify:
            sender.send_message("§cA WorldGen form cannot be used by an unavailable player.")
        return available

    def _release_form_lock(self, player_key: UUID, token: UUID) -> bool:
        if self.active_forms.get(player_key) != token:
            return False
        del self.active_forms[player_key]
        return True

    def _clear_form_lock(self, sender: CommandSender) -> None:
        player_key = self._form_player_key(sender)
        if player_key is not None:
            self.active_forms.pop(player_key, None)

    def _send_locked_form(
        self,
        sender: CommandSender,
        form: ActionForm | ModalForm,
        on_submit: Callable[[CommandSender, Any], None],
        on_close: Callable[[CommandSender], None] | None = None,
    ) -> bool:
        if not self._supports_forms(sender):
            sender.send_message("§cThe WorldGen menu is only available to players.")
            return False
        if not self._has_form_permission(sender):
            return False

        player_key = self._form_player_key(sender)
        if player_key is None:
            sender.send_message("§cThe player's stable UUID is unavailable; no form was opened.")
            return False
        if player_key in self.active_forms:
            sender.send_message("§eA WorldGen Studio form is already open.")
            return False
        if not self._form_player_available(sender):
            return False

        token = uuid4()
        self.active_forms[player_key] = token

        def submitted(callback_sender: CommandSender, response: Any) -> None:
            if self._form_player_key(callback_sender) != player_key:
                return
            if not self._release_form_lock(player_key, token):
                return
            if not self._form_player_available(callback_sender):
                return
            if not self._has_form_permission(callback_sender):
                return
            on_submit(callback_sender, response)

        def closed(callback_sender: CommandSender) -> None:
            if self._form_player_key(callback_sender) != player_key:
                return
            if not self._release_form_lock(player_key, token):
                return
            if on_close is None:
                return
            if not self._form_player_available(callback_sender):
                return
            if not self._has_form_permission(callback_sender):
                return
            on_close(callback_sender)

        form.on_submit = submitted
        form.on_close = closed
        try:
            sender.send_form(form)
        except Exception as error:
            self._release_form_lock(player_key, token)
            sender.send_message(f"§cUnable to open the WorldGen form: {error}")
            return False
        return True

    @staticmethod
    def _decode_modal_response(
        sender: CommandSender, response: Any, expected_values: int
    ) -> list[Any] | None:
        if not isinstance(response, str):
            sender.send_message("§cThe form returned a non-text response; no command was run.")
            return None
        try:
            values = json.loads(response)
        except (TypeError, ValueError):
            sender.send_message("§cThe form returned invalid JSON; no command was run.")
            return None
        if not isinstance(values, list) or len(values) != expected_values:
            sender.send_message(
                f"§cThe form response must contain exactly {expected_values} value(s); no command was run."
            )
            return None
        return values

    @staticmethod
    def _action_index(
        sender: CommandSender, response: Any, option_count: int
    ) -> int | None:
        if type(response) is not int or not 0 <= response < option_count:
            sender.send_message("§cThe form returned an invalid button selection; no command was run.")
            return None
        return response

    @staticmethod
    def _dropdown_index(
        sender: CommandSender, value: Any, option_count: int
    ) -> int | None:
        if type(value) is not int or not 0 <= value < option_count:
            sender.send_message("§cThe form returned an invalid dropdown selection; no command was run.")
            return None
        return value

    def _menu_target_args(
        self,
        sender: CommandSender,
        use_current_chunk: Any,
        chunk_x_text: Any,
        chunk_z_text: Any,
    ) -> list[str] | None:
        if type(use_current_chunk) is not bool:
            sender.send_message("§cThe current-chunk choice must be true or false; no command was run.")
            return None
        if type(chunk_x_text) is not str or type(chunk_z_text) is not str:
            sender.send_message("§cChunk coordinate fields must be text; no command was run.")
            return None
        if use_current_chunk:
            return []

        chunk_x_text = chunk_x_text.strip()
        chunk_z_text = chunk_z_text.strip()
        if not chunk_x_text or not chunk_z_text:
            sender.send_message("§cBoth chunk coordinates are required when current chunk is disabled.")
            return None
        try:
            chunk_x = int(chunk_x_text)
            chunk_z = int(chunk_z_text)
        except ValueError:
            sender.send_message("§cChunk coordinates must be integers; no command was run.")
            return None
        if not self._MIN_CHUNK_COORDINATE <= chunk_x <= self._MAX_CHUNK_COORDINATE or not (
            self._MIN_CHUNK_COORDINATE <= chunk_z <= self._MAX_CHUNK_COORDINATE
        ):
            sender.send_message("§cChunk coordinates are outside the adapter's safe world range.")
            return None
        return [str(chunk_x), str(chunk_z)]

    def _open_main_menu(self, sender: CommandSender) -> bool:
        form = ActionForm(
            title="§l§bWorldGen Studio",
            content=(
                "Choose a command group. Live generation and structures write to the current "
                "world after confirmation. Detached, benchmark, and inspect actions never edit "
                "the live world."
            ),
        )
        for label in (
            "§cLive Generation §7(World Write)",
            "§6Live Structures §7(World Write)",
            "§aDetached Buffers §7(No World Changes)",
            "§bBenchmark §7(No World Changes)",
            "§dInspect Buffer §7(No World Changes)",
            "§eNative Status",
        ):
            form.add_button(label)

        def selected(callback_sender: CommandSender, response: Any) -> None:
            selection = self._action_index(callback_sender, response, 6)
            if selection is None:
                self._open_main_menu(callback_sender)
                return
            if selection == 0:
                self._open_recipe_form(callback_sender, "gen")
            elif selection == 1:
                self._open_recipe_form(callback_sender, "structure")
            elif selection == 2:
                self._open_recipe_form(callback_sender, "buffer")
            elif selection == 3:
                self._open_benchmark_form(callback_sender)
            elif selection == 4:
                self._open_inspect_form(callback_sender)
            else:
                self._handle_status(callback_sender, [])

        return self._send_locked_form(sender, form, selected)

    def _open_recipe_form(
        self,
        sender: CommandSender,
        command_name: str,
        defaults: list[str] | None = None,
    ) -> bool:
        recipes = (
            self._STRUCTURE_RECIPES
            if command_name == "structure"
            else self._GENERATOR_RECIPES
        )
        if command_name not in {"gen", "structure", "buffer"}:
            raise ValueError(f"unsupported menu recipe command: {command_name}")

        if defaults is None:
            # A maze visibly rises above the player's floor. Flat can look
            # unchanged on ordinary grass and ores are intentionally hidden
            # underground, so neither is a useful live-menu default.
            defaults = ["maze" if command_name == "gen" else recipes[0]]
        else:
            defaults = list(defaults or [recipes[0]])
        default_recipe = defaults[0] if defaults and defaults[0] in recipes else recipes[0]
        use_current_chunk = len(defaults) == 1
        chunk_x = defaults[1] if len(defaults) == 3 else ""
        chunk_z = defaults[2] if len(defaults) == 3 else ""
        titles = {
            "gen": "Live Generation",
            "structure": "Live Structure",
            "buffer": "Detached Buffer",
        }
        submit_labels = {
            "gen": "Review Live Write",
            "structure": "Review Live Write",
            "buffer": "Generate Detached Buffer",
        }
        recipe_labels = {
            "gen": (
                "Flat Surface Patch",
                "Island at Floor Level",
                "Maze (Clearly Visible)",
                "Ores (Underground)",
            ),
            "structure": ("Castle (3x3 Chunks)", "Arena (3x3 Chunks)"),
            "buffer": tuple(f"{recipe.title()} (Detached Only)" for recipe in recipes),
        }
        form = ModalForm(
            title=f"§l{titles[command_name]}",
            controls=[
                Dropdown(
                    label="Recipe",
                    options=list(recipe_labels[command_name]),
                    default_index=recipes.index(default_recipe),
                ),
                Toggle(label="Use my current chunk", default_value=use_current_chunk),
                TextInput(
                    label="Chunk X (used when current chunk is off)",
                    placeholder="0",
                    default_value=chunk_x,
                ),
                TextInput(
                    label="Chunk Z (used when current chunk is off)",
                    placeholder="0",
                    default_value=chunk_z,
                ),
            ],
            submit_button=submit_labels[command_name],
        )

        def submitted(callback_sender: CommandSender, response: Any) -> None:
            values = self._decode_modal_response(callback_sender, response, 4)
            if values is None:
                self._open_recipe_form(callback_sender, command_name, defaults)
                return
            recipe_index = self._dropdown_index(callback_sender, values[0], len(recipes))
            target_args = self._menu_target_args(
                callback_sender, values[1], values[2], values[3]
            )
            if recipe_index is None or target_args is None:
                self._open_recipe_form(callback_sender, command_name, defaults)
                return
            handler_args = [recipes[recipe_index], *target_args]
            if command_name in {"gen", "structure"}:
                self._open_live_confirmation(
                    callback_sender, command_name, handler_args
                )
            else:
                self._handle_buffer(callback_sender, handler_args)

        return self._send_locked_form(
            sender,
            form,
            submitted,
            lambda callback_sender: self._open_main_menu(callback_sender),
        )

    def _open_live_confirmation(
        self, sender: CommandSender, command_name: str, handler_args: list[str]
    ) -> bool:
        recipe = handler_args[0]
        target_text = (
            "your current chunk"
            if len(handler_args) == 1
            else f"chunk ({handler_args[1]}, {handler_args[2]})"
        )
        form = ActionForm(
            title="§l§cConfirm Live World Write",
            content=(
                f"This will run /wg {command_name} {recipe} against {target_text}. "
                "The native service may change and flush blocks in the live world."
            ),
        )
        form.add_button("§cConfirm Live World Write")
        form.add_button("§7Back")

        def back(callback_sender: CommandSender) -> None:
            self._open_recipe_form(callback_sender, command_name, handler_args)

        def selected(callback_sender: CommandSender, response: Any) -> None:
            selection = self._action_index(callback_sender, response, 2)
            if selection is None:
                self._open_live_confirmation(
                    callback_sender, command_name, handler_args
                )
                return
            if selection == 0:
                handler_name = self._SUBCOMMAND_HANDLERS[command_name]
                getattr(self, handler_name)(callback_sender, list(handler_args))
            else:
                back(callback_sender)

        return self._send_locked_form(sender, form, selected, back)

    def _open_benchmark_form(self, sender: CommandSender) -> bool:
        form = ModalForm(
            title="§lDetached Buffer Benchmark",
            controls=[
                Toggle(label="Use default count (100)", default_value=True),
                TextInput(
                    label="Chunk count (1-128)",
                    placeholder="100",
                    default_value="100",
                ),
            ],
            submit_button="Run Detached Benchmark",
        )

        def submitted(callback_sender: CommandSender, response: Any) -> None:
            values = self._decode_modal_response(callback_sender, response, 2)
            if values is None:
                self._open_benchmark_form(callback_sender)
                return
            use_default, count_text = values
            if type(use_default) is not bool or type(count_text) is not str:
                callback_sender.send_message(
                    "§cBenchmark form values have invalid types; no command was run."
                )
                self._open_benchmark_form(callback_sender)
                return
            if use_default:
                handler_args: list[str] = []
            else:
                try:
                    count = int(count_text.strip())
                except ValueError:
                    count = 0
                if not 1 <= count <= 128:
                    callback_sender.send_message(
                        "§cChunk count must be an integer from 1 to 128; no command was run."
                    )
                    self._open_benchmark_form(callback_sender)
                    return
                handler_args = [str(count)]
            self._handle_benchmark(callback_sender, handler_args)

        return self._send_locked_form(
            sender,
            form,
            submitted,
            lambda callback_sender: self._open_main_menu(callback_sender),
        )

    def _open_inspect_form(self, sender: CommandSender) -> bool:
        form = ModalForm(
            title="§lInspect Detached Buffer",
            controls=[
                Toggle(label="Use my current chunk", default_value=True),
                TextInput(
                    label="Chunk X (used when current chunk is off)",
                    placeholder="0",
                    default_value="",
                ),
                TextInput(
                    label="Chunk Z (used when current chunk is off)",
                    placeholder="0",
                    default_value="",
                ),
            ],
            submit_button="Inspect Detached Buffer",
        )

        def submitted(callback_sender: CommandSender, response: Any) -> None:
            values = self._decode_modal_response(callback_sender, response, 3)
            if values is None:
                self._open_inspect_form(callback_sender)
                return
            handler_args = self._menu_target_args(
                callback_sender, values[0], values[1], values[2]
            )
            if handler_args is None:
                self._open_inspect_form(callback_sender)
                return
            self._handle_inspect(callback_sender, handler_args)

        return self._send_locked_form(
            sender,
            form,
            submitted,
            lambda callback_sender: self._open_main_menu(callback_sender),
        )

    def _require_live_bridge(self, sender: CommandSender) -> Any | None:
        bridge = getattr(self, "live_bridge", None)
        if bridge is None:
            bridge = self._connect_bridge()
        if bridge is None:
            reason = getattr(self, "bridge_error", "native bridge is unavailable")
            sender.send_message(f"§cNative WorldGen service unavailable: {reason}")
            sender.send_message(
                f"§7Install the matching v{self.version} platform wheel from the exact "
                "BDS bundle."
            )
            return None
        return bridge

    def _native_status(self, sender: CommandSender) -> dict | None:
        bridge = self._require_live_bridge(sender)
        if bridge is None:
            return None
        try:
            raw_status = dict(bridge.status(self.server))
        except Exception as error:
            sender.send_message(f"§cNative WorldGen status failed: {error}")
            return None
        status = {
            **raw_status,
            **dict(raw_status.get("stats") or {}),
            **dict(raw_status.get("diagnostics") or {}),
            **{
                f"live_{key}": value
                for key, value in dict(raw_status.get("live_stats") or {}).items()
            },
        }
        return status

    def _announce_reference_test(self, sender: CommandSender) -> None:
        sender.send_message(
            "§7Running a detached Python reference-buffer test; this command does not "
            "capture, change, or commit chunks in the live world."
        )

    @staticmethod
    def _dimension_name(sender: CommandSender) -> str | None:
        location = getattr(sender, "location", None)
        dimension = getattr(location, "dimension", None)
        name = getattr(dimension, "name", None)
        return str(name) if name else None

    def _get_target_chunk(
        self, sender: CommandSender, args: list[str]
    ) -> tuple[str, int, int, int] | None:
        if len(args) not in {0, 2}:
            sender.send_message("§cProvide both chunk coordinates or neither; partial/extra coordinates are rejected.")
            return None
        location = getattr(sender, "location", None)
        if location is None:
            sender.send_message(
                "§cThis command requires an in-game sender with a dimension; console defaults are disabled."
            )
            return None
        dimension = self._dimension_name(sender)
        if dimension is None:
            sender.send_message("§cThe sender's target dimension is unavailable.")
            return None

        if args:
            try:
                chunk_x, chunk_z = int(args[0]), int(args[1])
            except ValueError:
                sender.send_message("§cChunk coordinates must be integers.")
                return None
        else:
            try:
                chunk_x = math.floor(float(location.x)) // 16
                chunk_z = math.floor(float(location.z)) // 16
            except (TypeError, ValueError, OverflowError):
                sender.send_message("§cThe sender's location cannot be converted to chunk coordinates.")
                return None

        if not (-134_217_728 <= chunk_x <= 134_217_727) or not (
            -134_217_728 <= chunk_z <= 134_217_727
        ):
            sender.send_message("§cChunk coordinates are outside the adapter's safe world range.")
            return None
        try:
            anchor_y = math.floor(float(getattr(location, "y", None))) - 1
        except (TypeError, ValueError, OverflowError):
            sender.send_message("§cThe sender's Y location cannot be converted to a live recipe anchor.")
            return None
        if not (-2_147_483_648 <= anchor_y <= 2_147_483_647):
            sender.send_message("§cThe live recipe anchor Y is outside the native adapter's integer range.")
            return None
        return dimension, chunk_x, chunk_z, anchor_y

    def _run_live_recipe(
        self,
        sender: CommandSender,
        recipe: str,
        target: tuple[str, int, int, int],
    ) -> bool:
        bridge = self._require_live_bridge(sender)
        if bridge is None:
            return True
        dimension, chunk_x, chunk_z, anchor_y = target
        recipe_y_offsets = {
            "flat": (-1, 0),
            "island": (-4, 0),
            "maze": (0, 3),
            "castle": (0, 7),
            "arena": (0, 4),
        }
        if recipe == "ores":
            sender.send_message(
                f"§eApplying bounded live '{recipe}' recipe in {dimension} at "
                f"chunk ({chunk_x}, {chunk_z}); player anchor Y={anchor_y}. "
                "Ores scan natural stone/deepslate across the captured chunk and may write underground."
            )
        else:
            low_offset, high_offset = recipe_y_offsets[recipe]
            sender.send_message(
                f"§eApplying bounded live '{recipe}' recipe in {dimension} at "
                f"chunk ({chunk_x}, {chunk_z}); anchor surface/floor Y={anchor_y}; "
                f"bounded recipe band Y={anchor_y + low_offset}..{anchor_y + high_offset}."
            )
        try:
            result = dict(
                bridge.generate_live(
                    self.server, dimension, chunk_x, chunk_z, anchor_y, recipe
                )
            )
        except Exception as error:
            sender.send_message(f"§cLive recipe bridge call failed: {error}")
            return True

        try:
            changed = int(result.get("changed_blocks", 0))
            chunks = int(result.get("changed_chunks", 0))
            unconfirmed = int(result.get("unconfirmed_blocks", 0))
            if changed < 0 or chunks < 0 or unconfirmed < 0:
                raise ValueError("negative result counter")

            min_changed_y = result.get("min_changed_y")
            max_changed_y = result.get("max_changed_y")
            if (min_changed_y is None) != (max_changed_y is None):
                raise ValueError("only one changed-Y bound was returned")
            if min_changed_y is not None:
                if (
                    isinstance(min_changed_y, bool)
                    or isinstance(max_changed_y, bool)
                    or not isinstance(min_changed_y, int)
                    or not isinstance(max_changed_y, int)
                ):
                    raise ValueError("changed-Y bounds must be integers or both null")
                if min_changed_y > max_changed_y:
                    raise ValueError("changed-Y bounds are reversed")
                if not (
                    -2_147_483_648 <= min_changed_y <= 2_147_483_647
                    and -2_147_483_648 <= max_changed_y <= 2_147_483_647
                ):
                    raise ValueError("changed-Y bounds are outside the native integer range")
            if changed + unconfirmed > 0 and min_changed_y is None:
                raise ValueError("changed blocks were returned without a changed-Y range")
        except (TypeError, ValueError) as error:
            sender.send_message(f"§cLive recipe bridge returned an invalid result: {error}")
            return True

        changed_y_text = (
            "no native write range"
            if min_changed_y is None
            else (
                f"Y={min_changed_y}"
                if min_changed_y == max_changed_y
                else f"Y={min_changed_y}..{max_changed_y}"
            )
        )
        if result.get("success", False) and result.get("committed", False):
            sender.send_message(
                f"§aLive '{recipe}' recipe committed and flushed successfully: "
                f"{changed} block(s) changed across {chunks} chunk(s); "
                f"actual changed range {changed_y_text}."
            )
            return True
        if result.get("success", False) and changed == 0 and unconfirmed == 0:
            sender.send_message(
                f"§eLive '{recipe}' recipe completed with 0 changed blocks; "
                f"no commit was needed at anchor Y={anchor_y}, and there is no native write range."
            )
            return True

        failure = str(result.get("failure", "invalid_result"))
        message = str(result.get("message", "native service returned no reason"))
        sender.send_message(
            f"§cLive '{recipe}' recipe failed [{failure}] at anchor Y={anchor_y}; "
            f"native write range {changed_y_text}: {message}"
        )
        if changed or unconfirmed:
            sender.send_message(
                f"§7Confirmed changed blocks before failure: {changed}; "
                f"writes not confirmed by flush: {unconfirmed}."
            )
        return True

    def _handle_gen(self, sender: CommandSender, args: list[str]) -> bool:
        if (
            len(args) not in {1, 3}
            or args[0].lower() not in {"flat", "island", "maze", "ores"}
        ):
            sender.send_message("§cUsage: /wg gen <flat|island|maze|ores> [cx] [cz]")
            return True
        target = self._get_target_chunk(sender, args[1:])
        if target is None:
            return True
        return self._run_live_recipe(sender, args[0].lower(), target)

    def _handle_structure(self, sender: CommandSender, args: list[str]) -> bool:
        if len(args) not in {1, 3} or args[0].lower() not in {"castle", "arena"}:
            sender.send_message("§cUsage: /wg structure <castle|arena> [cx] [cz]")
            return True
        target = self._get_target_chunk(sender, args[1:])
        if target is None:
            return True
        return self._run_live_recipe(sender, args[0].lower(), target)

    def _handle_buffer(self, sender: CommandSender, args: list[str]) -> bool:
        if (
            len(args) not in {1, 3}
            or args[0].lower() not in {"flat", "island", "maze", "ores"}
        ):
            sender.send_message("§cUsage: /wg buffer <flat|island|maze|ores> [cx] [cz]")
            return True
        target = self._get_target_chunk(sender, args[1:])
        if target is None:
            return True
        self._announce_reference_test(sender)

        generator_name = args[0].lower()
        dimension, chunk_x, chunk_z, _anchor_y = target
        generators = {
            "flat": lambda: FlatGenerator(surface_y=64, base=1, top=2),
            "island": CustomIslandGenerator,
            "maze": CustomMazeGenerator,
            "ores": CustomOreGenerator,
        }
        generator = generators[generator_name]()
        context = GenerationContext(99999, dimension, ChunkPos(chunk_x, chunk_z))

        sender.send_message(
            f"§eGenerating detached reference buffer ({chunk_x}, {chunk_z}) "
            f"with '{generator_name}' and reference seed 99999..."
        )
        started = time.perf_counter()
        buffer = self.scheduler.generate(context, generator).result()
        elapsed_ms = (time.perf_counter() - started) * 1000.0
        sender.send_message(
            f"§aDetached buffer generated ({chunk_x}, {chunk_z}) in {elapsed_ms:.2f}ms. "
            f"Fingerprint: {hex(buffer.fingerprint())}"
        )
        return True

    def _handle_benchmark(self, sender: CommandSender, args: list[str]) -> bool:
        if len(args) > 1:
            sender.send_message("§cUsage: /wg benchmark [chunk_count]")
            return True
        try:
            count = int(args[0]) if args else 100
        except ValueError:
            sender.send_message("§cChunk count must be an integer from 1 to 128.")
            return True
        if not 1 <= count <= 128:
            sender.send_message("§cChunk count must be an integer from 1 to 128.")
            return True
        self._announce_reference_test(sender)

        sender.send_message(
            f"§eGenerating {count} detached buffers with 4 worker threads "
            "using reference seed 77777..."
        )
        started = time.perf_counter()
        generator = FlatGenerator(surface_y=64, base=1, top=2)
        futures = []
        for index in range(count):
            context = GenerationContext(
                77777,
                "overworld",
                ChunkPos(index % 16, index // 16),
            )
            futures.append(self.scheduler.generate(context, generator))
        results = [future.result() for future in futures]
        elapsed = max(time.perf_counter() - started, 1e-9)

        sender.send_message("§a=== WorldGen Benchmark Results ===")
        sender.send_message(f"  §7- Buffers Generated: §f{len(results)}")
        sender.send_message(f"  §7- Elapsed Time: §f{elapsed:.3f} seconds")
        sender.send_message(f"  §7- Throughput: §b{count / elapsed:.1f} buffers/sec")
        return True

    def _handle_inspect(self, sender: CommandSender, args: list[str]) -> bool:
        target = self._get_target_chunk(sender, args)
        if target is None:
            return True
        self._announce_reference_test(sender)
        dimension, chunk_x, chunk_z, _anchor_y = target
        context = GenerationContext(12345, dimension, ChunkPos(chunk_x, chunk_z))
        buffer = self.scheduler.generate(
            context, FlatGenerator(surface_y=64, base=1, top=2)
        ).result()

        sender.send_message(
            f"§e=== Inspecting Detached Reference Buffer ({chunk_x}, {chunk_z}); "
            "seed=12345 ==="
        )
        sender.send_message(f"  §7- Min Y: §f{buffer.min_y} §7Max Y: §f{buffer.max_y}")
        sender.send_message(f"  §7- Surface Y=64 Block: §b{buffer.get(0, 64, 0)}")
        sender.send_message(
            f"  §7- Bottom Y={buffer.min_y} Block: §b{buffer.get(0, buffer.min_y, 0)}"
        )
        sender.send_message(f"  §7- FNV-1a Fingerprint: §f{hex(buffer.fingerprint())}")
        return True

    def _handle_status(self, sender: CommandSender, args: list[str]) -> bool:
        if args:
            sender.send_message("§cUsage: /wg status")
            return True
        status = self._native_status(sender)
        if status is None:
            return True
        sender.send_message("§e=== Native WorldGen Service Status ===")
        sender.send_message(
            f"§7Interception Active: §f{status.get('interception_active', False)} "
            f"§7Populators: §f{status.get('populator_count', 0)}"
        )
        if not status.get("interception_active", False):
            sender.send_message(
                "§cAutomatic ChunkSource interception is inactive. Manual live recipes "
                "remain available through the exact capture/commit gate."
            )
        if int(status.get("populator_count", 0)) == 0:
            sender.send_message(
                "§eAutomatic interception is idle: no consumer IPopulator is registered. "
                "This is normal for the standalone API and does not affect /wg gen or /wg structure."
            )
        sender.send_message(
            f"§7Intercepted: §f{status.get('intercepted_requests', 0)} "
            f"§7Dispatched: §f{status.get('dispatched', 0)} "
            f"§7Committed: §f{status.get('committed', 0)}"
        )
        sender.send_message(
            f"§7Waiting: §f{status.get('waiting', 0)} "
            f"§7In-flight: §f{status.get('inflight', 0)} "
            f"§7Native dropped: §f{status.get('dropped_requests', 0)} "
            f"§7Waiting dropped: §f{status.get('waiting_overflow_drops', 0)}"
        )
        sender.send_message(
            f"§7Capture retries/failures: §f{status.get('capture_retries', 0)}/"
            f"{status.get('capture_failures', 0)} "
            f"§7Commit failures: §f{status.get('commit_failures', 0)} "
            f"§7Empty pipeline: §f{status.get('empty_pipeline_requests', 0)}"
        )
        sender.send_message(
            f"§7Exact build: §f{status.get('exact_build_match', False)} "
            f"§7Primary thread: §f{status.get('primary_thread', False)} "
            f"§7Hooked sources: §f{status.get('hooked_chunk_sources', 0)} "
            f"§7Chunk source available: §f{status.get('chunk_source_available', False)}"
        )
        sender.send_message(
            f"§7Manual live requests/success/failure: "
            f"§f{status.get('live_requests', 0)}/"
            f"{status.get('live_successful_requests', 0)}/"
            f"{status.get('live_failed_requests', 0)} "
            f"§7confirmed blocks: §f{status.get('live_changed_blocks', 0)} "
            f"§7unconfirmed: §f{status.get('live_unconfirmed_blocks', 0)}"
        )
        sender.send_message(
            f"§7Manual descriptor/capture/commit/flush/thread failures: "
            f"§f{status.get('live_descriptor_failures', 0)}/"
            f"{status.get('live_capture_failures', 0)}/"
            f"{status.get('live_commit_failures', 0)}/"
            f"{status.get('live_flush_failures', 0)}/"
            f"{status.get('live_primary_thread_rejections', 0)}"
        )
        return True
