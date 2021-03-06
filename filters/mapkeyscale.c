MFD_FILTER(mapkeyscale)

#ifdef MX_TTF

	mflt:mapkeyscale
	TTF_DEFAULTDEF("MIDI Keys Transpose")
	, TTF_IPORT( 0, "channelf", "Filter Channel",  0.0, 16.0,  0.0, PORTENUMZ("Any") DOC_CHANF)
	, TTF_IPORT( 1, "k0",  "C",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 2, "k1",  "C#", -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 3, "k2",  "D",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 4, "k3",  "D#", -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 5, "k4",  "E",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 6, "k5",  "F",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 7, "k6",  "F#", -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 8, "k7",  "G",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT( 9, "k8",  "G#", -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT(10, "k9",  "A",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT(11, "k10", "A#", -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	, TTF_IPORT(12, "k11", "B",  -13.0, 12.0, 0.0, lv2:portProperty lv2:integer; lv2:scalePoint [ rdfs:label "Off"; rdf:value  -13.0 ]; units:unit units:midiNote)
	; rdfs:comment "Flexible 12-tone map. Allow to map a note within an octave to another note in the same octave-range +- 12 semitones. Alternatively notes can also be masked (disabled). If two keys are moapped to the same note, the corresponding note on/events are latched: only the first note on and last note off will be sent. The settings can be changed dynamically: Note-on/off events will be sent accordingly.";
	.

#elif defined MX_CODE

static inline void filter_mapkeyscale_panic(MidiFilter* self, const uint8_t c, const uint32_t tme) {
	int k;
	for (k=0; k < 127; ++k) {
		if (self->memCS[c][k] > 0) {
			uint8_t buf[3];
			buf[0] = MIDI_NOTEOFF | c;
			buf[1] = k;
			buf[2] = 0;
			forge_midimessage(self, tme, buf, 3);
		}
		self->memCI[c][k] = -1000; // current key transpose
		self->memCM[c][k] = 0; // remember last velocity
		self->memCS[c][k] = 0; // count note-on per key
	}
}

static void
filter_midi_mapkeyscale(MidiFilter* self,
		uint32_t tme,
		const uint8_t* const buffer,
		uint32_t size)
{
	int i;
	const int chs = midi_limit_chn(floorf(*self->cfg[0]) -1);
	int keymap[12];
	for (i=0; i < 12; ++i) {
		keymap[i] = RAIL(floorf(*self->cfg[i+1]), -13, 12);
	}

	const uint8_t chn = buffer[0] & 0x0f;
	uint8_t mst = buffer[0] & 0xf0;

	if (midi_is_panic(buffer, size)) {
		filter_mapkeyscale_panic(self, chn, tme);
	}

	if (size != 3
			|| !(mst == MIDI_NOTEON || mst == MIDI_NOTEOFF || mst == MIDI_POLYKEYPRESSURE)
			|| !(floorf(*self->cfg[0]) == 0 || chs == chn)
			)
	{
		forge_midimessage(self, tme, buffer, size);
		return;
	}

	const uint8_t key = buffer[1] & 0x7f;
	const uint8_t vel = buffer[2] & 0x7f;

	if (mst == MIDI_NOTEON && vel ==0 ) {
		mst = MIDI_NOTEOFF;
	}

	int note;
	uint8_t buf[3];
	memcpy(buf, buffer, 3);

	switch (mst) {
		case MIDI_NOTEON:
			if (keymap[key%12] < -12) return;
			note = key + keymap[key%12];
			// TODO keep track of dup result note-on -- see enforcescale.c
			if (midi_valid(note)) {
				buf[1] = note;
				self->memCS[chn][note]++;
				if (self->memCS[chn][note] == 1) {
					forge_midimessage(self, tme, buf, size);
				}
				self->memCM[chn][key] = vel;
				self->memCI[chn][key] = note - key;
			}
			break;
		case MIDI_NOTEOFF:
			note = key + self->memCI[chn][key];
			if (midi_valid(note)) {
				buf[1] = note;
				if (self->memCS[chn][note] > 0) {
					self->memCS[chn][note]--;
					if (self->memCS[chn][note] == 0)
						forge_midimessage(self, tme, buf, size);
				}
			}
			self->memCM[chn][key] = 0;
			self->memCI[chn][key] = -1000;
			break;
		case MIDI_POLYKEYPRESSURE:
			if (keymap[key%12] < -12) return;
			note = key + keymap[key%12];
			if (midi_valid(note)) {
				buf[1] = note;
				forge_midimessage(self, tme, buf, size);
			}
			break;
	}
}

static void filter_preproc_mapkeyscale(MidiFilter* self) {
	int i;
	int identical_cfg = 1;
	int keymap[12];
	for (i=0; i < 12; ++i) {
		keymap[i] = RAIL(floorf(*self->cfg[i+1]), -13, 12);
		if (floorf(self->lcfg[i+1]) != floorf(*self->cfg[i+1])) {
			identical_cfg = 0;
		}
	}
	if (identical_cfg) return;

	int c,k;
	uint8_t buf[3];
	buf[2] = 0;
	for (c=0; c < 16; ++c) {
		for (k=0; k < 127; ++k) {
			int note;
			const int n = 1 + k%12;
			if (!self->memCM[c][k]) continue;
			if (floorf(self->lcfg[n]) == floorf(*self->cfg[n])) continue;

			note = k + self->memCI[c][k];

			if (midi_valid(note)) {
				note = midi_limit_val(note);
				if (self->memCS[c][note] > 0) {
					self->memCS[c][note]--;
					if (self->memCS[c][note] == 0) {
						buf[0] = MIDI_NOTEOFF | c;
						buf[1] = note;
						buf[2] = 0;
						forge_midimessage(self, 0, buf, 3);
					}
				}
			}

			note = k + keymap[k%12];

			if (midi_valid(note)) {
				note = midi_limit_val(note);
				buf[0] = MIDI_NOTEON | c;
				buf[1] = note;
				buf[2] = self->memCM[c][k];
				self->memCI[c][k] = note - k;
				self->memCS[c][note]++;
				if (self->memCS[c][note] == 1) {
					forge_midimessage(self, 0, buf, 3);
				}
			} else {
				self->memCM[c][k] = 0;
				self->memCI[c][k] = -1000;
			}
		}
	}
}

static void filter_init_mapkeyscale(MidiFilter* self) {
	int c,k;
	for (c=0; c < 16; ++c) for (k=0; k < 127; ++k) {
		self->memCI[c][k] = -1000; // current key transpose
		self->memCM[c][k] = 0; // remember last velocity
		self->memCS[c][k] = 0; // count note-on per key
	}
	self->preproc_fn = filter_preproc_mapkeyscale;
}

#endif
