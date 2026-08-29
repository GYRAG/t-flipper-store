#include "input_handling.h"
#include "calculator_state.h"
#include "calculator.h"
#include "utilities.h"
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void calculator_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/* T-Embed: one-axis dial, so Up/Down alone could only ever reach one column of
 * the keypad. Walk every key linearly in reading order with wrap. The grid is
 * ragged - row 0 is the lone MODE key and row 4 has no fifth key - so step over
 * cells getKeyAtPosition() reports as blank rather than letting the cursor
 * settle on one. Left/Right do the same, so the hold-and-turn modifier is not
 * needed to reach a key. */
#define PC_GRID_W 5
#define PC_GRID_H 5

static void pc_step_key(Calculator* calculator_state, int delta) {
    int idx = calculator_state->position.y * PC_GRID_W + calculator_state->position.x;
    for(int guard = 0; guard < PC_GRID_W * PC_GRID_H; guard++) {
        idx = (idx + delta + PC_GRID_W * PC_GRID_H) % (PC_GRID_W * PC_GRID_H);
        short x = idx % PC_GRID_W, y = idx / PC_GRID_W;
        if(getKeyAtPosition(x, y) != ' ') {
            calculator_state->position.x = x;
            calculator_state->position.y = y;
            return;
        }
    }
}

void handle_short_press(Calculator* calculator_state, ViewPort* view_port, InputEvent* event) {
    switch(event->key) {
    case InputKeyUp:
    case InputKeyLeft:
        pc_step_key(calculator_state, -1);
        break;
    case InputKeyDown:
    case InputKeyRight:
        pc_step_key(calculator_state, +1);
        break;
    case InputKeyOk:
        if(calculator_state->position.y == 0) {
            toggle_mode(calculator_state);
        } else {
            char key =
                getKeyAtPosition(calculator_state->position.x, calculator_state->position.y);
            handle_key_press(calculator_state, key);
        }
        break;

    default:
        break;
    }

    view_port_update(view_port);
}

void handle_long_press(Calculator* calculator_state, ViewPort* view_port, InputEvent* event) {
    // Handling a long press event
    switch(event->key) {
    case InputKeyOk: {
        const char* inputMessage = "  github  armixz";
        strncpy(calculator_state->text, inputMessage, MAX_TEXT_LENGTH_INPUT - 1);
        calculator_state->text[MAX_TEXT_LENGTH_INPUT - 1] = '\0';
        calculator_state->textLength = strlen(calculator_state->text);
        calculator_state->newInputStarted = true;
        view_port_update(view_port);
    } break;

    default:
        break;
    }
}

void handle_key_press(Calculator* calculator_state, char key) {
    switch(key) {
    case '=':
        // Logic for '=' key
        strncpy(calculator_state->originalInput, calculator_state->text, MAX_TEXT_LENGTH_INPUT);
        calculate(calculator_state);
        // calculator_state->text[0] = '\0';
        calculator_state->textLength = 0;
        break;
    case 'R':
        // Logic for 'R' key, typically 'Clear'
        calculator_state->text[0] = '\0';
        calculator_state->textLength = 0;
        calculator_state->decToBinResult[0] = '\0';
        calculator_state->decToHexResult[0] = '\0';
        calculator_state->decToCharResult[0] = '\0';
        calculator_state->hexToBinResult[0] = '\0';
        calculator_state->hexToDecResult[0] = '\0';
        calculator_state->binToDecResult[0] = '\0';
        calculator_state->binToHexResult[0] = '\0';
        calculator_state->newInputStarted = false;
        break;
    case '<':
        // Logic for '<' key, typically 'Backspace'
        if(calculator_state->textLength > 0) {
            calculator_state->text[--calculator_state->textLength] = '\0';
        }
        calculator_state->newInputStarted = false;
        break;
    default:
        // Default logic for number and operator keys
        if(calculator_state->newInputStarted) {
            // Reset the text for a fresh input if new input has started
            calculator_state->text[0] = '\0';
            calculator_state->textLength = 0;
            calculator_state->newInputStarted = false;
        }
        // Add the new character to the text, respecting the maximum text length
        if(calculator_state->textLength < MAX_TEXT_LENGTH_INPUT - 1) {
            calculator_state->text[calculator_state->textLength++] = key;
            calculator_state->text[calculator_state->textLength] = '\0';
        }
        break;
    }
}

void handle_event(Calculator* calculator_state, ViewPort* view_port, InputEvent* event) {
    if(event->type == InputTypeShort) {
        handle_short_press(calculator_state, view_port, event);
    } else if(event->type == InputTypeLong) {
        handle_long_press(calculator_state, view_port, event);
    }
    view_port_update(view_port);
}
