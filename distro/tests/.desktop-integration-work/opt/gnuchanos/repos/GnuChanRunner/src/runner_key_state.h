/*
 * runner_key_state.h — tracking keys that were held when the launcher opened.
 */
#ifndef GNUCHANRUNNER_KEY_STATE_H
#define GNUCHANRUNNER_KEY_STATE_H

#define RUNNER_KEYMAP_SIZE 32
#define RUNNER_KEYCODE_LIMIT (RUNNER_KEYMAP_SIZE * 8)

typedef struct RunnerKeyState {
    unsigned char held_at_open[RUNNER_KEYMAP_SIZE];
} RunnerKeyState;

static inline void runner_key_state_capture(
    RunnerKeyState *state, const char keymap[RUNNER_KEYMAP_SIZE]) {
    for (int i = 0; i < RUNNER_KEYMAP_SIZE; i++) {
        state->held_at_open[i] = (unsigned char)keymap[i];
    }
}

static inline int runner_key_state_ignores_press(
    const RunnerKeyState *state, unsigned int keycode) {
    if (keycode >= RUNNER_KEYCODE_LIMIT) {
        return 0;
    }
    return (state->held_at_open[keycode / 8] &
            (1u << (keycode % 8))) != 0;
}

static inline void runner_key_state_release(
    RunnerKeyState *state, unsigned int keycode, int auto_repeat) {
    if (auto_repeat || keycode >= RUNNER_KEYCODE_LIMIT) {
        return;
    }
    state->held_at_open[keycode / 8] &=
        (unsigned char)~(1u << (keycode % 8));
}

#endif
