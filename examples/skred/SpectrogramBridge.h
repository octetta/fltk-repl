#ifndef SKRED_SPECTROGRAM_BRIDGE_H
#define SKRED_SPECTROGRAM_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Slot 9 is reserved by the FLTK front end for audio bitmap data transfer. */
int skred_spectrogram_bind(void);
void skred_spectrogram_unbind(void);

/* Queue a spectrogram from a wavetable or completed recording. Returns zero
 * when sample data was accepted, or -1 when the source produced no data. */
int skred_spectrogram_wave(int wave);
int skred_spectrogram_record(int channel);
int skred_waveform_wave(int wave);
int skred_waveform_record(int channel);

#ifdef __cplusplus
}
#endif

#endif
