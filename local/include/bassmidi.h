/*
	BASSMIDI 2.4 C/C++ header file
	Copyright (c) 2006-2018 Un4seen Developments Ltd.

	See the BASSMIDI.CHM file for more detailed documentation
*/

#ifndef BASSMIDI_H
#define BASSMIDI_H

#include "bass.h"

#if BASSVERSION!=0x204
#error conflicting BASS and BASSMIDI versions
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BASSMIDIDEF
#define BASSMIDIDEF(f) WINAPI f
#endif

typedef DWORD HSOUNDFONT;	// soundfont handle

// Additional error codes returned by BASS_ErrorGetCode
#define BASS_ERROR_MIDI_INCLUDE		7000	// SFZ include file could not be opened

// Additional BASS_SetConfig options
#define BASS_CONFIG_MIDI_COMPACT	0x10400
#define BASS_CONFIG_MIDI_VOICES		0x10401
#define BASS_CONFIG_MIDI_AUTOFONT	0x10402
#define BASS_CONFIG_MIDI_IN_PORTS	0x10404

// Additional BASS_SetConfigPtr options
#define BASS_CONFIG_MIDI_DEFFONT	0x10403

// Additional sync types
#define BASS_SYNC_MIDI_MARK		0x10000
#define BASS_SYNC_MIDI_MARKER	0x10000
#define BASS_SYNC_MIDI_CUE		0x10001
#define BASS_SYNC_MIDI_LYRIC	0x10002
#define BASS_SYNC_MIDI_TEXT		0x10003
#define BASS_SYNC_MIDI_EVENT	0x10004
#define BASS_SYNC_MIDI_TICK		0x10005
#define BASS_SYNC_MIDI_TIMESIG	0x10006
#define BASS_SYNC_MIDI_KEYSIG	0x10007

// Additional BASS_MIDI_StreamCreateFile/etc flags
#define BASS_MIDI_NOSYSRESET	0x800
#define BASS_MIDI_DECAYEND		0x1000
#define BASS_MIDI_NOFX			0x2000
#define BASS_MIDI_DECAYSEEK		0x4000
#define BASS_MIDI_NOCROP		0x8000
#define BASS_MIDI_NOTEOFF1		0x10000
#define BASS_MIDI_SINCINTER		0x800000

// BASS_MIDI_FontInit flags
#define BASS_MIDI_FONT_MEM		0x10000
#define BASS_MIDI_FONT_MMAP		0x20000
#define BASS_MIDI_FONT_XGDRUMS	0x40000
#define BASS_MIDI_FONT_NOFX		0x80000
#define BASS_MIDI_FONT_LINATTMOD	0x100000

typedef struct {
	HSOUNDFONT font;	// soundfont
	int preset;			// preset number (-1=all)
	int bank;
} BASS_MIDI_FONT;

typedef struct {
	HSOUNDFONT font;	// soundfont
	int spreset;		// source preset number
	int sbank;			// source bank number
	int dpreset;		// destination preset/program number
	int dbank;			// destination bank number
	int dbanklsb;		// destination bank number LSB
} BASS_MIDI_FONTEX;

// BASS_MIDI_StreamSet/GetFonts flag
#define BASS_MIDI_FONT_EX		0x1000000	// BASS_MIDI_FONTEX

typedef struct {
	const char *name;
	const char *copyright;
	const char *comment;
	DWORD presets;		// number of presets/instruments
	DWORD samsize;		// total size (in bytes) of the sample data
	DWORD samload;		// amount of sample data currently loaded
	DWORD samtype;		// sample format (CTYPE) if packed
} BASS_MIDI_FONTINFO;

typedef struct {
	DWORD track;		// track containing marker
	DWORD pos;			// marker position
	const char *text;	// marker text
} BASS_MIDI_MARK;

// Marker types
#define BASS_MIDI_MARK_MARKER	0	// marker
#define BASS_MIDI_MARK_CUE		1	// cue point
#define BASS_MIDI_MARK_LYRIC	2	// lyric
#define BASS_MIDI_MARK_TEXT		3	// text
#define BASS_MIDI_MARK_TIMESIG	4	// time signature
#define BASS_MIDI_MARK_KEYSIG	5	// key signature
#define BASS_MIDI_MARK_COPY		6	// copyright notice
#define BASS_MIDI_MARK_TRACK	7	// track name
#define BASS_MIDI_MARK_INST		8	// instrument name
#define BASS_MIDI_MARK_TRACKSTART	9	// track start (SMF2)
#define BASS_MIDI_MARK_TICK		0x10000 // flag: get position in ticks (otherwise bytes)

// MIDI events
#define MIDI_EVENT_NOTE				1
#define MIDI_EVENT_PROGRAM			2
#define MIDI_EVENT_CHANPRES			3
#define MIDI_EVENT_PITCH			4
#define MIDI_EVENT_PITCHRANGE		5
#define MIDI_EVENT_DRUMS			6
#define MIDI_EVENT_FINETUNE			7
#define MIDI_EVENT_COARSETUNE		8
#define MIDI_EVENT_MASTERVOL		9
#define MIDI_EVENT_BANK				10
#define MIDI_EVENT_MODULATION		11
#define MIDI_EVENT_VOLUME			12
#define MIDI_EVENT_PAN				13
#define MIDI_EVENT_EXPRESSION		14
#define MIDI_EVENT_SUSTAIN			15
#define MIDI_EVENT_SOUNDOFF			16
#define MIDI_EVENT_RESET			17
#define MIDI_EVENT_NOTESOFF			18
#define MIDI_EVENT_PORTAMENTO		19
#define MIDI_EVENT_PORTATIME		20
#define MIDI_EVENT_PORTANOTE		21
#define MIDI_EVENT_MODE				22
#define MIDI_EVENT_REVERB			23
#define MIDI_EVENT_CHORUS			24
#define MIDI_EVENT_CUTOFF			25
#define MIDI_EVENT_RESONANCE		26
#define MIDI_EVENT_RELEASE			27
#define MIDI_EVENT_ATTACK			28
#define MIDI_EVENT_DECAY			29
#define MIDI_EVENT_REVERB_MACRO		30
#define MIDI_EVENT_CHORUS_MACRO		31
#define MIDI_EVENT_REVERB_TIME		32
#define MIDI_EVENT_REVERB_DELAY		33
#define MIDI_EVENT_REVERB_LOCUTOFF	34
#define MIDI_EVENT_REVERB_HICUTOFF	35
#define MIDI_EVENT_REVERB_LEVEL		36
#define MIDI_EVENT_CHORUS_DELAY		37
#define MIDI_EVENT_CHORUS_DEPTH		38
#define MIDI_EVENT_CHORUS_RATE		39
#define MIDI_EVENT_CHORUS_FEEDBACK	40
#define MIDI_EVENT_CHORUS_LEVEL		41
#define MIDI_EVENT_CHORUS_REVERB	42
#define MIDI_EVENT_USERFX			43
#define MIDI_EVENT_USERFX_LEVEL		44
#define MIDI_EVENT_USERFX_REVERB	45
#define MIDI_EVENT_USERFX_CHORUS	46
#define MIDI_EVENT_DRUM_FINETUNE	50
#define MIDI_EVENT_DRUM_COARSETUNE	51
#define MIDI_EVENT_DRUM_PAN			52
#define MIDI_EVENT_DRUM_REVERB		53
#define MIDI_EVENT_DRUM_CHORUS		54
#define MIDI_EVENT_DRUM_CUTOFF		55
#define MIDI_EVENT_DRUM_RESONANCE	56
#define MIDI_EVENT_DRUM_LEVEL		57
#define MIDI_EVENT_DRUM_USERFX		58
#define MIDI_EVENT_SOFT				60
#define MIDI_EVENT_SYSTEM			61
#define MIDI_EVENT_TEMPO			62
#define MIDI_EVENT_SCALETUNING		63
#define MIDI_EVENT_CONTROL			64
#define MIDI_EVENT_CHANPRES_VIBRATO	65
#define MIDI_EVENT_CHANPRES_PITCH	66
#define MIDI_EVENT_CHANPRES_FILTER	67
#define MIDI_EVENT_CHANPRES_VOLUME	68
#define MIDI_EVENT_MOD_VIBRATO		69
#define MIDI_EVENT_MODRANGE			69
#define MIDI_EVENT_BANK_LSB			70
#define MIDI_EVENT_KEYPRES			71
#define MIDI_EVENT_KEYPRES_VIBRATO	72
#define MIDI_EVENT_KEYPRES_PITCH	73
#define MIDI_EVENT_KEYPRES_FILTER	74
#define MIDI_EVENT_KEYPRES_VOLUME	75
#define MIDI_EVENT_SOSTENUTO		76
#define MIDI_EVENT_MOD_PITCH		77
#define MIDI_EVENT_MOD_FILTER		78
#define MIDI_EVENT_MOD_VOLUME		79
#define MIDI_EVENT_MIXLEVEL			0x10000
#define MIDI_EVENT_TRANSPOSE		0x10001
#define MIDI_EVENT_SYSTEMEX			0x10002
#define MIDI_EVENT_SPEED			0x10004

#define MIDI_EVENT_END				0
#define MIDI_EVENT_END_TRACK		0x10003

#define MIDI_EVENT_NOTES			0x20000
#define MIDI_EVENT_VOICES			0x20001

#define MIDI_SYSTEM_DEFAULT			0
#define MIDI_SYSTEM_GM1				1
#define MIDI_SYSTEM_GM2				2
#define MIDI_SYSTEM_XG				3
#define MIDI_SYSTEM_GS				4

typedef struct {
	DWORD event;		// MIDI_EVENT_xxx
	DWORD param;
	DWORD chan;
	DWORD tick;			// event position (ticks)
	DWORD pos;			// event position (bytes)
} BASS_MIDI_EVENT;

// BASS_MIDI_StreamEvents modes
#define BASS_MIDI_EVENTS_STRUCT		0 // BASS_MIDI_EVENT structures
#define BASS_MIDI_EVENTS_RAW		0x10000 // raw MIDI event data
#define BASS_MIDI_EVENTS_SYNC		0x1000000 // flag: trigger event syncs
#define BASS_MIDI_EVENTS_NORSTATUS	0x2000000 // flag: no running status
#define BASS_MIDI_EVENTS_CANCEL		0x4000000 // flag: cancel pending events
#define BASS_MIDI_EVENTS_TIME		0x8000000 // flag: delta-time info is present
#define BASS_MIDI_EVENTS_ABSTIME	0x10000000 // flag: absolute time info is present

// BASS_MIDI_StreamGetChannel special channels
#define BASS_MIDI_CHAN_CHORUS		(DWORD)-1
#define BASS_MIDI_CHAN_REVERB		(DWORD)-2
#define BASS_MIDI_CHAN_USERFX		(DWORD)-3

// BASS_CHANNELINFO type
#define BASS_CTYPE_STREAM_MIDI	0x10d00

// Additional attributes
#define BASS_ATTRIB_MIDI_PPQN		0x12000
#define BASS_ATTRIB_MIDI_CPU		0x12001
#define BASS_ATTRIB_MIDI_CHANS		0x12002
#define BASS_ATTRIB_MIDI_VOICES		0x12003
#define BASS_ATTRIB_MIDI_VOICES_ACTIVE 0x12004
#define BASS_ATTRIB_MIDI_STATE		0x12005
#define BASS_ATTRIB_MIDI_SRC		0x12006
#define BASS_ATTRIB_MIDI_KILL		0x12007
#define BASS_ATTRIB_MIDI_TRACK_VOL	0x12100 // + track #

// Additional tag type
#define BASS_TAG_MIDI_TRACK	0x11000	// + track #, track text : array of null-terminated ANSI strings

// BASS_ChannelGetLength/GetPosition/SetPosition mode
#define BASS_POS_MIDI_TICK		2		// tick position

typedef BOOL (CALLBACK MIDIFILTERPROC)(HSTREAM handle, DWORD track, BASS_MIDI_EVENT *event, BOOL seeking, void *user);
/* Event filtering callback function.
handle : MIDI stream handle
track  : Track containing the event
event  : The event
seeking: TRUE = the event is being processed while seeking, FALSE = it is being played
user   : The 'user' parameter value given when calling BASS_MIDI_StreamSetFilter
RETURN : TRUE = process the event, FALSE = drop the event */

// BASS_MIDI_FontPack flags
#define BASS_MIDI_PACK_NOHEAD		1	// don't send a WAV header to the encoder
#define BASS_MIDI_PACK_16BIT		2	// discard low 8 bits of 24-bit sample data

typedef struct {
	const char *name;	// description
	DWORD id;
	DWORD flags;
} BASS_MIDI_DEVICEINFO;

typedef void (CALLBACK MIDIINPROC)(DWORD device, double time, const BYTE *buffer, DWORD length, void *user);
/* MIDI input callback function.
device : MIDI input device
time   : Timestamp
buffer : Buffer containing MIDI data
length : Number of bytes of data
user   : The 'user' parameter value given when calling BASS_MIDI_InInit */

HSTREAM BASSMIDIDEF(BASS_MIDI_StreamCreate)(DWORD channels, DWORD flags, DWORD freq);
HSTREAM BASSMIDIDEF(BASS_MIDI_StreamCreateFile)(BOOL mem, const void *file, QWORD offset, QWORD length, DWORD flags, DWORD freq);
HSTREAM BASSMIDIDEF(BASS_MIDI_StreamCreateURL)(const char *url, DWORD offset, DWORD flags, DOWNLOADPROC *proc, void *user, DWORD freq);
HSTREAM BASSMIDIDEF(BASS_MIDI_StreamCreateFileUser)(DWORD system, DWORD flags, const BASS_FILEPROCS *procs, void *user, DWORD freq);
HSTREAM BASSMIDIDEF(BASS_MIDI_StreamCreateEvents)(const BASS_MIDI_EVENT *events, DWORD ppqn, DWORD flags, DWORD freq);
BOOL BASSMIDIDEF(BASS_MIDI_StreamGetMark)(HSTREAM handle, DWORD type, DWORD index, BASS_MIDI_MARK *mark);
DWORD BASSMIDIDEF(BASS_MIDI_StreamGetMarks)(HSTREAM handle, int track, DWORD type, BASS_MIDI_MARK *marks);
BOOL BASSMIDIDEF(BASS_MIDI_StreamSetFonts)(HSTREAM handle, const void *fonts, DWORD count);
DWORD BASSMIDIDEF(BASS_MIDI_StreamGetFonts)(HSTREAM handle, void *fonts, DWORD count);
BOOL BASSMIDIDEF(BASS_MIDI_StreamLoadSamples)(HSTREAM handle);
BOOL BASSMIDIDEF(BASS_MIDI_StreamEvent)(HSTREAM handle, DWORD chan, DWORD event, DWORD param);
DWORD BASSMIDIDEF(BASS_MIDI_StreamEvents)(HSTREAM handle, DWORD mode, const void *events, DWORD length);
DWORD BASSMIDIDEF(BASS_MIDI_StreamGetEvent)(HSTREAM handle, DWORD chan, DWORD event);
DWORD BASSMIDIDEF(BASS_MIDI_StreamGetEvents)(HSTREAM handle, int track, DWORD filter, BASS_MIDI_EVENT *events);
DWORD BASSMIDIDEF(BASS_MIDI_StreamGetEventsEx)(HSTREAM handle, int track, DWORD filter, BASS_MIDI_EVENT *events, DWORD start, DWORD count);
BOOL BASSMIDIDEF(BASS_MIDI_StreamGetPreset)(HSTREAM handle, DWORD chan, BASS_MIDI_FONT *font);
HSTREAM BASSMIDIDEF(BASS_MIDI_StreamGetChannel)(HSTREAM handle, DWORD chan);
BOOL BASSMIDIDEF(BASS_MIDI_StreamSetFilter)(HSTREAM handle, BOOL seeking, MIDIFILTERPROC *proc, void *user);

HSOUNDFONT BASSMIDIDEF(BASS_MIDI_FontInit)(const void *file, DWORD flags);
HSOUNDFONT BASSMIDIDEF(BASS_MIDI_FontInitUser)(const BASS_FILEPROCS *procs, void *user, DWORD flags);
BOOL BASSMIDIDEF(BASS_MIDI_FontFree)(HSOUNDFONT handle);
BOOL BASSMIDIDEF(BASS_MIDI_FontGetInfo)(HSOUNDFONT handle, BASS_MIDI_FONTINFO *info);
BOOL BASSMIDIDEF(BASS_MIDI_FontGetPresets)(HSOUNDFONT handle, DWORD *presets);
const char *BASSMIDIDEF(BASS_MIDI_FontGetPreset)(HSOUNDFONT handle, int preset, int bank);
BOOL BASSMIDIDEF(BASS_MIDI_FontLoad)(HSOUNDFONT handle, int preset, int bank);
BOOL BASSMIDIDEF(BASS_MIDI_FontUnload)(HSOUNDFONT handle, int preset, int bank);
BOOL BASSMIDIDEF(BASS_MIDI_FontCompact)(HSOUNDFONT handle);
BOOL BASSMIDIDEF(BASS_MIDI_FontPack)(HSOUNDFONT handle, const void *outfile, const void *encoder, DWORD flags);
BOOL BASSMIDIDEF(BASS_MIDI_FontUnpack)(HSOUNDFONT handle, const void *outfile, DWORD flags);
BOOL BASSMIDIDEF(BASS_MIDI_FontSetVolume)(HSOUNDFONT handle, float volume);
float BASSMIDIDEF(BASS_MIDI_FontGetVolume)(HSOUNDFONT handle);

DWORD BASSMIDIDEF(BASS_MIDI_ConvertEvents)(const BYTE *data, DWORD length, BASS_MIDI_EVENT *events, DWORD count, DWORD flags);

BOOL BASSMIDIDEF(BASS_MIDI_InGetDeviceInfo)(DWORD device, BASS_MIDI_DEVICEINFO *info);
BOOL BASSMIDIDEF(BASS_MIDI_InInit)(DWORD device, MIDIINPROC *proc, void *user);
BOOL BASSMIDIDEF(BASS_MIDI_InFree)(DWORD device);
BOOL BASSMIDIDEF(BASS_MIDI_InStart)(DWORD device);
BOOL BASSMIDIDEF(BASS_MIDI_InStop)(DWORD device);

#ifdef __cplusplus
}

static inline BOOL BASS_MIDI_StreamSetFonts(HSTREAM handle, const BASS_MIDI_FONTEX *fonts, DWORD count)
{
	return BASS_MIDI_StreamSetFonts(handle, (const void*)fonts, count|BASS_MIDI_FONT_EX);
}

static inline DWORD BASS_MIDI_StreamGetFonts(HSTREAM handle, BASS_MIDI_FONTEX *fonts, DWORD count)
{
	return BASS_MIDI_StreamGetFonts(handle, (void*)fonts, count|BASS_MIDI_FONT_EX);
}

#ifdef _WIN32
static inline HSTREAM BASS_MIDI_StreamCreateFile(BOOL mem, const WCHAR *file, QWORD offset, QWORD length, DWORD flags, DWORD freq)
{
	return BASS_MIDI_StreamCreateFile(mem, (const void*)file, offset, length, flags|BASS_UNICODE, freq);
}

static inline HSTREAM BASS_MIDI_StreamCreateURL(const WCHAR *url, DWORD offset, DWORD flags, DOWNLOADPROC *proc, void *user, DWORD freq)
{
	return BASS_MIDI_StreamCreateURL((const char*)url, offset, flags|BASS_UNICODE, proc, user, freq);
}

static inline HSOUNDFONT BASS_MIDI_FontInit(const WCHAR *file, DWORD flags)
{
	return BASS_MIDI_FontInit((const void*)file, flags|BASS_UNICODE);
}

static inline BOOL BASS_MIDI_FontPack(HSOUNDFONT handle, const WCHAR *outfile, const WCHAR *encoder, DWORD flags)
{
	return BASS_MIDI_FontPack(handle, (const void*)outfile, (const void*)encoder, flags|BASS_UNICODE);
}

static inline BOOL BASS_MIDI_FontUnpack(HSOUNDFONT handle, const WCHAR *outfile, DWORD flags)
{
	return BASS_MIDI_FontUnpack(handle, (const void*)outfile, flags|BASS_UNICODE);
}
#endif
#endif

#endif
