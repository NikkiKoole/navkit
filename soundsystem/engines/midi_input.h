// midi_input.h — CoreMIDI keyboard input (macOS)
// Gracefully no-ops when no MIDI device is connected.
// Include once, call midiInputInit/Poll/Shutdown from your main loop.

#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H

#include <stdbool.h>
#include <stdint.h>

// MIDI event types
typedef enum {
    MIDI_NOTE_ON,
    MIDI_NOTE_OFF,
    MIDI_CC,           // Control Change (mod wheel, sustain pedal, etc.)
    MIDI_PITCH_BEND,
} MidiEventType;

typedef struct {
    MidiEventType type;
    uint8_t channel;   // 0-15
    uint8_t data1;     // note number (note on/off), CC number, pitch bend LSB
    uint8_t data2;     // velocity (note on/off), CC value, pitch bend MSB
} MidiEvent;

// Ring buffer for incoming MIDI events (lock-free single producer / single consumer)
#define MIDI_EVENT_BUFFER_SIZE 256

typedef struct {
    MidiEvent events[MIDI_EVENT_BUFFER_SIZE];
    volatile int writePos;
    volatile int readPos;
    bool initialized;
    bool deviceConnected;
    char deviceName[128];
} MidiInputState;

static MidiInputState g_midiInput = {0};

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize MIDI input. No-op if no device found. Safe to call always.
static void midiInputInit(void);

// Poll for pending MIDI events. Returns number of events written to `out`.
// Non-blocking — returns 0 if no events available.
static int midiInputPoll(MidiEvent *out, int maxEvents);

// Clean up. Safe to call even if Init was a no-op.
static void midiInputShutdown(void);

// Query state
static bool midiInputIsConnected(void);
static const char *midiInputDeviceName(void);

// ============================================================================
// IMPLEMENTATION
// ============================================================================

#ifdef __APPLE__

#include <CoreMIDI/CoreMIDI.h>
#include <stdio.h>
#include <string.h>

static MIDIClientRef   g_midiClient   = 0;
static MIDIPortRef     g_midiPort     = 0;
static MIDIEndpointRef g_midiSource   = 0;

// Push an event into the ring buffer (called from CoreMIDI callback thread)
static void midiInput_pushEvent(MidiEvent ev) {
    int next = (g_midiInput.writePos + 1) % MIDI_EVENT_BUFFER_SIZE;
    if (next == g_midiInput.readPos) return; // buffer full, drop event
    g_midiInput.events[g_midiInput.writePos] = ev;
    g_midiInput.writePos = next;
}

// CoreMIDI read callback — called on a CoreMIDI thread
static void midiInput_readProc(const MIDIPacketList *pktList, void *readProcRefCon, void *srcConnRefCon) {
    (void)readProcRefCon;
    (void)srcConnRefCon;

    const MIDIPacket *pkt = &pktList->packet[0];
    for (UInt32 i = 0; i < pktList->numPackets; i++) {
        // Parse MIDI bytes — handle running status and multi-byte messages
        for (UInt16 j = 0; j < pkt->length; j++) {
            uint8_t byte = pkt->data[j];
            if (byte & 0x80) {
                // Status byte
                uint8_t status  = byte & 0xF0;
                uint8_t channel = byte & 0x0F;

                if (status == 0x90 && j + 2 < pkt->length) {
                    // Note On
                    uint8_t note = pkt->data[j + 1];
                    uint8_t vel  = pkt->data[j + 2];
                    MidiEvent ev;
                    ev.channel = channel;
                    ev.data1 = note;
                    ev.data2 = vel;
                    if (vel == 0) {
                        ev.type = MIDI_NOTE_OFF;
                    } else {
                        ev.type = MIDI_NOTE_ON;
                    }
                    midiInput_pushEvent(ev);
                    j += 2;
                } else if (status == 0x80 && j + 2 < pkt->length) {
                    // Note Off
                    MidiEvent ev;
                    ev.type = MIDI_NOTE_OFF;
                    ev.channel = channel;
                    ev.data1 = pkt->data[j + 1];
                    ev.data2 = pkt->data[j + 2];
                    midiInput_pushEvent(ev);
                    j += 2;
                } else if (status == 0xB0 && j + 2 < pkt->length) {
                    // Control Change
                    MidiEvent ev;
                    ev.type = MIDI_CC;
                    ev.channel = channel;
                    ev.data1 = pkt->data[j + 1]; // CC number
                    ev.data2 = pkt->data[j + 2]; // CC value
                    midiInput_pushEvent(ev);
                    j += 2;
                } else if (status == 0xE0 && j + 2 < pkt->length) {
                    // Pitch Bend
                    MidiEvent ev;
                    ev.type = MIDI_PITCH_BEND;
                    ev.channel = channel;
                    ev.data1 = pkt->data[j + 1]; // LSB
                    ev.data2 = pkt->data[j + 2]; // MSB
                    midiInput_pushEvent(ev);
                    j += 2;
                }
                // Skip other status bytes (sysex, clock, etc.)
            }
        }
        pkt = MIDIPacketNext(pkt);
    }
}

// Notification callback for device connect/disconnect
static void midiInput_notifyProc(const MIDINotification *msg, void *refCon) {
    (void)refCon;

    if (msg->messageID == kMIDIMsgObjectAdded) {
        // A new device appeared — try to connect if we don't have one
        if (!g_midiInput.deviceConnected && g_midiPort) {
            ItemCount numSources = MIDIGetNumberOfSources();
            if (numSources > 0) {
                g_midiSource = MIDIGetSource(0);
                OSStatus err = MIDIPortConnectSource(g_midiPort, g_midiSource, NULL);
                if (err == noErr) {
                    g_midiInput.deviceConnected = true;
                    // Get device name
                    CFStringRef name = NULL;
                    MIDIObjectGetStringProperty(g_midiSource, kMIDIPropertyDisplayName, &name);
                    if (name) {
                        CFStringGetCString(name, g_midiInput.deviceName,
                                           sizeof(g_midiInput.deviceName), kCFStringEncodingUTF8);
                        CFRelease(name);
                    }
                    printf("[MIDI] Connected: %s\n", g_midiInput.deviceName);
                }
            }
        }
    } else if (msg->messageID == kMIDIMsgObjectRemoved) {
        // Check if our source is still valid
        ItemCount numSources = MIDIGetNumberOfSources();
        bool found = false;
        for (ItemCount i = 0; i < numSources; i++) {
            if (MIDIGetSource(i) == g_midiSource) {
                found = true;
                break;
            }
        }
        if (!found && g_midiInput.deviceConnected) {
            g_midiInput.deviceConnected = false;
            g_midiInput.deviceName[0] = '\0';
            g_midiSource = 0;
            printf("[MIDI] Disconnected\n");
        }
    }
}

static void midiInputInit(void) {
    memset(&g_midiInput, 0, sizeof(g_midiInput));

    // Create MIDI client
    OSStatus err = MIDIClientCreate(CFSTR("PixelSynth"),
                                     midiInput_notifyProc, NULL,
                                     &g_midiClient);
    if (err != noErr) {
        printf("[MIDI] No MIDI support available (CoreMIDI error %d)\n", (int)err);
        return;
    }

    // Create input port
    err = MIDIInputPortCreate(g_midiClient, CFSTR("PixelSynth Input"),
                               midiInput_readProc, NULL,
                               &g_midiPort);
    if (err != noErr) {
        printf("[MIDI] Could not create input port\n");
        MIDIClientDispose(g_midiClient);
        g_midiClient = 0;
        return;
    }

    g_midiInput.initialized = true;

    // Connect to first available source (if any)
    ItemCount numSources = MIDIGetNumberOfSources();
    if (numSources == 0) {
        printf("[MIDI] No MIDI devices found (plug one in anytime — hot-plug supported)\n");
        return;
    }

    g_midiSource = MIDIGetSource(0);
    err = MIDIPortConnectSource(g_midiPort, g_midiSource, NULL);
    if (err != noErr) {
        printf("[MIDI] Could not connect to source\n");
        return;
    }

    g_midiInput.deviceConnected = true;

    // Get device name
    CFStringRef name = NULL;
    MIDIObjectGetStringProperty(g_midiSource, kMIDIPropertyDisplayName, &name);
    if (name) {
        CFStringGetCString(name, g_midiInput.deviceName,
                           sizeof(g_midiInput.deviceName), kCFStringEncodingUTF8);
        CFRelease(name);
    } else {
        snprintf(g_midiInput.deviceName, sizeof(g_midiInput.deviceName), "MIDI Source 0");
    }

    printf("[MIDI] Connected: %s\n", g_midiInput.deviceName);
}

static int midiInputPoll(MidiEvent *out, int maxEvents) {
    int count = 0;
    while (count < maxEvents && g_midiInput.readPos != g_midiInput.writePos) {
        out[count++] = g_midiInput.events[g_midiInput.readPos];
        g_midiInput.readPos = (g_midiInput.readPos + 1) % MIDI_EVENT_BUFFER_SIZE;
    }
    return count;
}

static void midiInputShutdown(void) {
    if (g_midiPort && g_midiSource) {
        MIDIPortDisconnectSource(g_midiPort, g_midiSource);
    }
    if (g_midiPort) {
        MIDIPortDispose(g_midiPort);
        g_midiPort = 0;
    }
    if (g_midiClient) {
        MIDIClientDispose(g_midiClient);
        g_midiClient = 0;
    }
    g_midiInput.initialized = false;
    g_midiInput.deviceConnected = false;
}

static bool midiInputIsConnected(void) {
    return g_midiInput.deviceConnected;
}

static const char *midiInputDeviceName(void) {
    if (g_midiInput.deviceConnected) return g_midiInput.deviceName;
    return "No MIDI device";
}

#else
// ============================================================================
// NON-APPLE STUBS — compile cleanly, do nothing
// ============================================================================

static void midiInputInit(void) {
    memset(&g_midiInput, 0, sizeof(g_midiInput));
}

static int midiInputPoll(MidiEvent *out, int maxEvents) {
    (void)out; (void)maxEvents;
    return 0;
}

static void midiInputShutdown(void) {}
static bool midiInputIsConnected(void) { return false; }
static const char *midiInputDeviceName(void) { return "MIDI not supported"; }

#endif // __APPLE__

// ============================================================================
// MIDI LEARN — map any CC to any float parameter
// ============================================================================

#define MIDI_LEARN_MAX_MAPPINGS 64

typedef struct {
    float *target;     // pointer to the parameter
    float min, max;    // parameter range
    int cc;            // MIDI CC number (0-127)
    char label[32];    // display name
    bool active;
} MidiCCMapping;

typedef struct {
    MidiCCMapping mappings[MIDI_LEARN_MAX_MAPPINGS];
    int count;

    // Learn mode state
    bool learning;         // waiting for a CC
    float *learnTarget;    // which parameter is waiting
    float learnMin, learnMax;
    char learnLabel[32];
} MidiLearnState;

static MidiLearnState g_midiLearn = {0};

// Arm a parameter for MIDI learn (call when user right-clicks a knob).
// If already waiting, cancels. If already mapped, removes the mapping.
static void midiLearnArm(float *target, float min, float max, const char *label) {
    // If already waiting on this same parameter, cancel learn
    if (g_midiLearn.learning && g_midiLearn.learnTarget == target) {
        g_midiLearn.learning = false;
        g_midiLearn.learnTarget = NULL;
        return;
    }

    // Check if already mapped — if so, unlearn it
    for (int i = 0; i < g_midiLearn.count; i++) {
        if (g_midiLearn.mappings[i].active && g_midiLearn.mappings[i].target == target) {
            // Remove mapping by swapping with last
            g_midiLearn.mappings[i] = g_midiLearn.mappings[g_midiLearn.count - 1];
            g_midiLearn.mappings[g_midiLearn.count - 1].active = false;
            g_midiLearn.count--;
            g_midiLearn.learning = false;
            return;
        }
    }

    // Arm for learning
    g_midiLearn.learning = true;
    g_midiLearn.learnTarget = target;
    g_midiLearn.learnMin = min;
    g_midiLearn.learnMax = max;
    if (label) {
        snprintf(g_midiLearn.learnLabel, sizeof(g_midiLearn.learnLabel), "%s", label);
    } else {
        g_midiLearn.learnLabel[0] = '\0';
    }
}

// Cancel learn mode without mapping
static void midiLearnCancel(void) {
    g_midiLearn.learning = false;
    g_midiLearn.learnTarget = NULL;
}

// Check if a parameter is in learn-waiting state
static bool midiLearnIsWaiting(float *target) {
    return g_midiLearn.learning && g_midiLearn.learnTarget == target;
}

// Get the CC number mapped to a parameter, or -1 if not mapped
static int midiLearnGetCC(float *target) {
    for (int i = 0; i < g_midiLearn.count; i++) {
        if (g_midiLearn.mappings[i].active && g_midiLearn.mappings[i].target == target) {
            return g_midiLearn.mappings[i].cc;
        }
    }
    return -1;
}

// Process a CC event: complete learn if pending, or apply mapped value.
// Call this for every MIDI_CC event. Returns true if the CC was consumed.
static bool midiLearnProcessCC(int cc, float value01) {
    // If learning, bind this CC
    if (g_midiLearn.learning && g_midiLearn.learnTarget) {
        // Remove any existing mapping for this CC (one CC = one param)
        for (int i = 0; i < g_midiLearn.count; i++) {
            if (g_midiLearn.mappings[i].active && g_midiLearn.mappings[i].cc == cc) {
                g_midiLearn.mappings[i] = g_midiLearn.mappings[g_midiLearn.count - 1];
                g_midiLearn.mappings[g_midiLearn.count - 1].active = false;
                g_midiLearn.count--;
                break;
            }
        }

        if (g_midiLearn.count < MIDI_LEARN_MAX_MAPPINGS) {
            MidiCCMapping *m = &g_midiLearn.mappings[g_midiLearn.count];
            m->target = g_midiLearn.learnTarget;
            m->min = g_midiLearn.learnMin;
            m->max = g_midiLearn.learnMax;
            m->cc = cc;
            m->active = true;
            snprintf(m->label, sizeof(m->label), "%s", g_midiLearn.learnLabel);
            g_midiLearn.count++;
        }

        // Apply the current value immediately
        *g_midiLearn.learnTarget = g_midiLearn.learnMin +
            value01 * (g_midiLearn.learnMax - g_midiLearn.learnMin);

        g_midiLearn.learning = false;
        g_midiLearn.learnTarget = NULL;
        return true;
    }

    // Apply to existing mappings
    bool consumed = false;
    for (int i = 0; i < g_midiLearn.count; i++) {
        MidiCCMapping *m = &g_midiLearn.mappings[i];
        if (m->active && m->cc == cc) {
            *m->target = m->min + value01 * (m->max - m->min);
            consumed = true;
        }
    }
    return consumed;
}

// Clear all mappings
static void midiLearnClearAll(void) {
    g_midiLearn.count = 0;
    g_midiLearn.learning = false;
    g_midiLearn.learnTarget = NULL;
}

#endif // MIDI_INPUT_H
