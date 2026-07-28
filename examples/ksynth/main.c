/* =========================================================================
 * krepl - K-Synth Array DSP REPL  v0.6
 *
 * fltk-repl front-end for the K-Synth engine with panel DSL support.
 *
 * NEW in 0.6:
 *   - Runtime-configurable polyphony via \V N and --voices N
 *   - Panel files may embed @ksynth ... @end blocks (multi-line init code)
 *     evaluated at panel-load time; stripped before passing to panel loader.
 *   - @skred ... @end blocks are logged as "not available in krepl"
 *     (reserved for future skrepl interop or scripted panel init)
 *
 * Voice pitch/gain syntax (shared by \p and \P):
 *   [+/-semitones[:cents]] [gain]
 *   e.g.  \P 1 +7       (perfect fifth up)
 *         \P 1 -12:0 0.5 (octave down, half volume)
 *         \p A +3:50 0.8  (3 semitones + 50 cents, 80% volume)
 *
 * Embedded panel code blocks:
 *   @ksynth                           <- evaluates as krepl input lines
 *   A = sin(440*!44100/44100*2*3.14)
 *   \A 1 A
 *   @end
 *
 *   @skred                            <- not available here; logged & skipped
 *   some skred code
 *   @end
 *
 * REPL commands:
 *   \p [s] VAR [pitch] [gain]   play variable
 *   \q                          stop all voices
 *   \x                          voice status
 *   \V N                        set polyphony (1-64, stops all voices)
 *   \A N VAR [s]                assign var to slot 1-16
 *   \P N [pitch] [gain]         play slot N
 *   \b                          list all 16 slots
 *   \k N                        clear slot N
 *   \s [s] VAR                  save WAV
 *   \c VAR                      export C header
 *   \v [VAR]                    list/inspect variables
 *   \W VAR                      waveform bitmap window
 *   \Z VAR                      spectrogram bitmap window
 *   \l [FILE]                   load .ks script (picker if no arg)
 *   \w MS                       wait milliseconds
 *   \t                          toggle scope-after-eval
 *   \?                          help
 *
 *   panel load|open|reload|show|hide|list|set|get|dump ...
 * ========================================================================= */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "repl/repl_api.h"
#include "repl/panel_dsl.h"
#include "repl/bitmap_win.h"
#include "ksynth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

/* ---- global state -------------------------------------------------------- */

#define NUM_SLOTS    16
#define DEFAULT_VOICES 8
#define MAX_VOICES_CAP 64

/*
 * Voice: phase is a fractional *frame* index.
 * rate  = pitch multiplier (1.0 = natural, 2^(1/12) = +1 semitone).
 * gain  = amplitude scale.
 */
typedef struct {
    K      buffer;
    double phase;
    double rate;
    float  gain;
    int    n_frames;
    int    stereo;
    int    active;
} Voice;

typedef struct {
    float *buf;
    int    len;
    int    stereo;
    char   label[48];
} Slot;

static Voice  *g_voices   = NULL;   /* malloc'd; size = g_n_voices */
static int     g_n_voices = 0;
static Slot    g_slots[NUM_SLOTS];
static ks_ctx *g_ctx      = NULL;
static repl_ctx *g_repl   = NULL;
static ma_device g_device;
static int     g_show     = 0;
static int     g_audio_ok = 0;

/* ---- polyphony ----------------------------------------------------------- */

static int set_polyphony(int n) {
    if (n < 1) n = 1;
    if (n > MAX_VOICES_CAP) n = MAX_VOICES_CAP;
    /* stop + free all running voices first */
    for (int i = 0; i < g_n_voices; i++) {
        g_voices[i].active = 0;
        if (g_voices[i].buffer) { k_free(g_ctx, g_voices[i].buffer); g_voices[i].buffer = NULL; }
    }
    Voice *nv = (Voice *)realloc(g_voices, (size_t)n * sizeof(Voice));
    if (!nv) return -1;
    /* zero-initialise any newly added slots */
    if (n > g_n_voices) memset(nv + g_n_voices, 0, (size_t)(n - g_n_voices) * sizeof(Voice));
    g_voices   = nv;
    g_n_voices = n;
    return 0;
}

/* ---- audio callback (linear-interpolated, pitch+gain aware) -------------- */

static void audio_cb(ma_device *d, void *out, const void *in, ma_uint32 n) {
    (void)d; (void)in;
    float *o = (float *)out;
    for (ma_uint32 j = 0; j < n * 2; j++) o[j] = 0.0f;

    for (int v = 0; v < g_n_voices; v++) {
        if (!g_voices[v].active) continue;
        K buf = g_voices[v].buffer;
        if (!buf) { g_voices[v].active = 0; continue; }

        double phase  = g_voices[v].phase;
        double rate   = g_voices[v].rate;
        float  gain   = g_voices[v].gain;
        int    nf     = g_voices[v].n_frames;
        int    stereo = g_voices[v].stereo;

        for (ma_uint32 j = 0; j < n; j++) {
            if (phase >= nf) { g_voices[v].active = 0; break; }
            int    f0 = (int)phase;
            int    f1 = f0 + 1 < nf ? f0 + 1 : nf - 1;
            double fr = phase - f0;
            if (stereo) {
                float L = (float)(buf->f[f0*2]   * (1.0-fr) + buf->f[f1*2]   * fr) * gain;
                float R = (float)(buf->f[f0*2+1] * (1.0-fr) + buf->f[f1*2+1] * fr) * gain;
                o[j*2] += L; o[j*2+1] += R;
            } else {
                float s = (float)(buf->f[f0] * (1.0-fr) + buf->f[f1] * fr) * gain;
                o[j*2] += s; o[j*2+1] += s;
            }
            phase += rate;
        }
        g_voices[v].phase = phase;
    }
}

/* ---- pitch/gain arg parser ----------------------------------------------- */

static const char *parse_pitch_gain(const char *arg, double *out_rate, float *out_gain) {
    *out_rate = 1.0; *out_gain = 1.0;
    while (*arg == ' ') arg++;
    if (*arg == '+' || *arg == '-') {
        int sign = (*arg == '-') ? -1 : 1; arg++;
        int semi = 0;
        while (isdigit((unsigned char)*arg)) semi = semi*10 + (*arg++ - '0');
        int cents = 0;
        if (*arg == ':') { arg++; while (isdigit((unsigned char)*arg)) cents = cents*10 + (*arg++ - '0'); }
        *out_rate = pow(2.0, sign * (semi + cents / 100.0) / 12.0);
        while (*arg == ' ') arg++;
    }
    if (*arg && (isdigit((unsigned char)*arg) || *arg == '.')) {
        *out_gain = (float)atof(arg);
        while (*arg && !isspace((unsigned char)*arg)) arg++;
    }
    return arg;
}

/* ---- voice start helper -------------------------------------------------- */

static void voice_start(K buf, int stereo, double rate, float gain) {
    if (!g_audio_ok) { repl_println(g_repl, "/ audio not available"); return; }
    int vslot = -1;
    for (int i = 0; i < g_n_voices; i++)
        if (!g_voices[i].active) { vslot = i; break; }
    if (vslot < 0) { repl_printf(g_repl, "/ no free voice slots (polyphony=%d)\n", g_n_voices); return; }
    if (g_voices[vslot].buffer) k_free(g_ctx, g_voices[vslot].buffer);
    g_voices[vslot].buffer   = buf;
    g_voices[vslot].phase    = 0.0;
    g_voices[vslot].rate     = rate;
    g_voices[vslot].gain     = gain;
    g_voices[vslot].n_frames = stereo ? buf->n / 2 : buf->n;
    g_voices[vslot].stereo   = stereo;
    g_voices[vslot].active   = 1;

    double semis_f = log2(rate) * 12.0;
    int semis = (int)round(semis_f);
    int centi = (int)round((semis_f - semis) * 100.0);
    if (semis == 0 && centi == 0)
        repl_printf(g_repl, "/ voice %d: %s  rate=1.0  gain=%.2f  %.1fms\n",
            vslot, stereo ? "stereo" : "mono  ", gain,
            (double)g_voices[vslot].n_frames / 44100.0 * 1000.0);
    else
        repl_printf(g_repl, "/ voice %d: %s  %+d semitones %+d cents  gain=%.2f  %.1fms\n",
            vslot, stereo ? "stereo" : "mono  ", semis, centi, gain,
            (double)g_voices[vslot].n_frames / 44100.0 * 1000.0);
}

/* ---- braille scope ------------------------------------------------------- */

static void draw_line_grid(unsigned char *grid,
                           int x1, int y1, int x2, int y2, int w, int h) {
    int dx = abs(x2-x1), sx = x1<x2?1:-1, dy = -abs(y2-y1), sy = y1<y2?1:-1;
    int err = dx+dy, e2;
    while (1) {
        if (x1>=0&&x1<w&&y1>=0&&y1<h) grid[y1*w+x1]=1;
        if (x1==x2&&y1==y2) break;
        e2=2*err;
        if (e2>=dy){err+=dy;x1+=sx;} if (e2<=dx){err+=dx;y1+=sy;}
    }
}

static void print_scope_to_repl(double *data, int len, int width, int height) {
    if (len < 2) return;
    double min_y = data[0], max_y = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] < min_y) min_y = data[i];
        if (data[i] > max_y) max_y = data[i];
    }
    if (max_y == min_y) { max_y += 0.1; min_y -= 0.1; }
    int cw = (width/2)*2, ch = (height/4)*4;
    unsigned char *grid = (unsigned char *)calloc((size_t)cw*(size_t)ch, 1);
    if (!grid) return;
    int prev_y = -1;
    for (int x = 0; x < cw; x++) {
        double pos = (double)x/(cw-1)*(len-1);
        int lo=(int)floor(pos), hi=(int)ceil(pos); if (hi>=len) hi=len-1;
        double val = data[lo]*(1.0-(pos-lo))+data[hi]*(pos-lo);
        int y = (int)((max_y-val)/(max_y-min_y)*(ch-1));
        if (prev_y != -1) draw_line_grid(grid, x-1, prev_y, x, y, cw, ch);
        else if (y>=0&&y<ch) grid[y*cw+x]=1;
        prev_y = y;
    }
    char hdr[256];
    snprintf(hdr,sizeof(hdr),"\x1b[38;5;240m  MAX: %-10.4f  DUR: %.4fms / %d samples\x1b[0m",
             max_y, (double)len/44100.0*1000.0, len);
    repl_println(g_repl, hdr);
    char border[1024]="  \xe2\x94\x8c\x1b[38;5;244m";
    for (int i=0;i<cw/2;i++) strcat(border,"\xe2\x94\x80");
    strcat(border,"\x1b[0m\xe2\x94\x90"); repl_println(g_repl, border);
    static const int dots[8][2]={{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{3,0},{3,1}};
    static const int bits[8]={1,2,4,8,16,32,64,128};
    for (int y=0;y<ch;y+=4) {
        char row[2048]="  \x1b[38;5;244m\xe2\x94\x82\x1b[0m";
        double rv_top=max_y-((double)y/(ch-1))*(max_y-min_y);
        double rv_bot=max_y-((double)(y+4)/(ch-1))*(max_y-min_y);
        int has_zero=(rv_top>=0&&rv_bot<=0);
        for (int x=0;x<cw;x+=2) {
            unsigned int mask=0;
            for (int i=0;i<8;i++) {
                int dy=y+dots[i][0],dx=x+dots[i][1];
                if (dy<ch&&dx<cw&&grid[dy*cw+dx]) mask|=(unsigned)bits[i];
            }
            if (mask==0) {
                strcat(row, has_zero?"\x1b[38;5;244m\xe2\x80\xa5\x1b[0m":" ");
            } else {
                unsigned int cp=0x2800u+mask;
                char cell[8];
                cell[0]=(char)(0xE0|(cp>>12)); cell[1]=(char)(0x80|((cp>>6)&0x3F));
                cell[2]=(char)(0x80|(cp&0x3F)); cell[3]='\0';
                strcat(row,"\x1b[38;5;34m"); strcat(row,cell); strcat(row,"\x1b[0m");
            }
        }
        strcat(row,"\x1b[38;5;244m\xe2\x94\x82\x1b[0m"); repl_println(g_repl, row);
    }
    char bot[1024]="  \xe2\x94\x94\x1b[38;5;244m";
    for (int i=0;i<cw/2;i++) strcat(bot,"\xe2\x94\x80");
    strcat(bot,"\x1b[0m\xe2\x94\x98"); repl_println(g_repl, bot);
    free(grid);
}

/* ---- helpers ------------------------------------------------------------- */

static float *k_to_float(K v) {
    if (!v||v->n<=0) return NULL;
    float *buf=(float *)malloc((size_t)v->n*sizeof(float));
    if (!buf) return NULL;
    for (int i=0;i<v->n;i++) buf[i]=(float)v->f[i];
    return buf;
}

static char get_var_name(const char *p) {
    while (*p==' ') p++;
    char c=*p;
    if (c>='a'&&c<='z') c=(char)(c-'a'+'A');
    if (c>='A'&&c<='Z') return c;
    return '\0';
}

static void show_var(char v_name) {
    K v=g_ctx->vars[v_name-'A'];
    if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
    repl_printf(g_repl,"\x1b[36m%c\x1b[0m [%d samples / %.2fms] (",
                v_name,v->n,(double)v->n/44100.0*1000.0);
    int lim=v->n<10?v->n:10;
    for (int i=0;i<lim;i++) repl_printf(g_repl,"%.4f%s",v->f[i],i==lim-1?"":" ");
    repl_println(g_repl,v->n>10?" ...)":")");;
    print_scope_to_repl(v->f,v->n,128,32);
}

static int write_wav(const char *filename, double *data, ma_uint64 frames,
                     ma_uint32 chans, ma_uint32 sr) {
    ma_encoder enc;
    ma_encoder_config cfg=ma_encoder_config_init(ma_encoding_format_wav,ma_format_f32,chans,sr);
    if (ma_encoder_init_file(filename,&cfg,&enc)!=MA_SUCCESS) return -1;
    float tmp[4096*2]; ma_uint64 done=0;
    while (done<frames) {
        ma_uint64 chunk=frames-done; if (chunk>4096) chunk=4096;
        for (ma_uint64 i=0;i<chunk*chans;i++) tmp[i]=(float)data[done*chans+i];
        ma_uint64 written; ma_encoder_write_pcm_frames(&enc,tmp,chunk,&written); done+=written;
    }
    ma_encoder_uninit(&enc);
    repl_printf(g_repl,"/ wrote %lld frames to %s\n",(long long)done,filename);
    return 0;
}

/* ---- forward declarations ------------------------------------------------ */
static void handle_line(const char *line);
static void handle_line_single(const char *line);
static void handle_ks_file(const char *path);

/* ---- panel embedded-code pre-processor ----------------------------------- */
/*
 * Reads a .pnl file. Any @ksynth ... @end blocks are evaluated immediately
 * as krepl input. @skred ... @end blocks are noted but skipped.
 * All other lines are written to a temp file, which is returned (caller frees
 * the path string and should unlink() after use). Returns NULL on error.
 */
static char *preprocess_panel(const char *path) {
    FILE *fin = fopen(path, "r");
    if (!fin) { repl_printf(g_repl, "/ cannot open panel: %s\n", path); return NULL; }

    char tmppath[256];
    snprintf(tmppath, sizeof(tmppath), "/tmp/krepl_panel_%lld.pnl", (long long)time(NULL));
    FILE *fout = fopen(tmppath, "w");
    if (!fout) { fclose(fin); repl_printf(g_repl, "/ cannot write temp panel file\n"); return NULL; }

    char line[4096];
    int  in_block = 0;  /* 0=none, 1=ksynth, 2=skred */

    while (fgets(line, sizeof(line), fin)) {
        /* trim trailing newline for comparison, keep copy for output */
        size_t len = strlen(line);
        char trimmed[4096];
        memcpy(trimmed, line, len + 1);
        /* strip trailing \r\n */
        while (len > 0 && (trimmed[len-1] == '\n' || trimmed[len-1] == '\r'))
            trimmed[--len] = '\0';

        /* leading whitespace for directive check */
        const char *t = trimmed;
        while (*t == ' ' || *t == '\t') t++;

        if (strncmp(t, "@ksynth", 7) == 0 && (t[7]=='\0'||isspace((unsigned char)t[7]))) {
            in_block = 1;
            repl_printf(g_repl, "/ panel @ksynth block\n");
            continue;
        }
        if (strncmp(t, "@skred", 6) == 0 && (t[6]=='\0'||isspace((unsigned char)t[6]))) {
            in_block = 2;
            repl_printf(g_repl, "/ panel @skred block (skipped in krepl)\n");
            continue;
        }
        if (strcmp(t, "@end") == 0) {
            if (in_block == 1) repl_printf(g_repl, "/ @ksynth block done\n");
            in_block = 0;
            continue;
        }

        if (in_block == 1) {
            /* evaluate as krepl input */
            handle_line(trimmed);
        } else if (in_block == 2) {
            /* skip silently */
        } else {
            /* normal panel DSL line — pass through */
            fputs(line, fout);
        }
    }

    fclose(fin);
    fclose(fout);
    return strdup(tmppath);
}

/* ---- panel error/list callbacks ------------------------------------------ */

static void panel_error_cb(const char *msg, void *ud) {
    (void)ud;
    if (g_repl && msg) repl_printf(g_repl, "\x1b[31m/ panel: %s\x1b[0m\n", msg);
}

static void panel_list_cb(const char *name, const char *path,
                          const char *params, int is_shown, void *ud) {
    (void)ud; (void)params;
    repl_printf(g_repl, "  %-16s  %s  %s\n", name,
                is_shown?"\x1b[32mvisible\x1b[0m":"\x1b[38;5;240mhidden \x1b[0m", path);
}

static void panel_ctrl_cb(const char *ctrl, const char *val, void *ud) {
    (void)ud;
    repl_printf(g_repl, "  %-20s = %s\n", ctrl, val);
}

/* Panel button → krepl evaluator */
static void panel_to_krepl(const char *line, void *ud) {
    (void)ud;
    if (!line || !*line) return;
    repl_printf(g_repl, "\x1b[38;5;244m/ panel> %s\x1b[0m\n", line);
    handle_line(line);
}

/* ---- panel load with pre-processing -------------------------------------- */

static void krepl_panel_load(const char *pname, const char *ppath, const char *params) {
    char *tmp = preprocess_panel(ppath);
    if (!tmp) return;
    panel_win_t *pw = (params && params[0])
        ? panel_registry_load_params(pname, tmp, params)
        : panel_registry_load(pname, tmp);
    unlink(tmp);  /* clean up temp file */
    free(tmp);
    if (pw) {
        panel_registry_show(pname);
        repl_printf(g_repl, "/ panel '%s' loaded from %s\n", pname, ppath);
    } else {
        repl_printf(g_repl, "/ panel load failed: %s\n", ppath);
    }
}

/* ---- panel REPL subcommand ----------------------------------------------- */

static void handle_panel_cmd(const char *rest) {
    char sub[32]={0}; int consumed=0;
    if (sscanf(rest, "%31s%n", sub, &consumed) < 1) {
        repl_println(g_repl, "/ usage: panel load|open|reload|show|hide|list|set|get|dump");
        return;
    }
    const char *arg = rest + consumed; while (*arg == ' ') arg++;

    if (strcmp(sub, "load") == 0) {
        char pname[64]={0}, ppath[256]={0}, params[256]={0}; int nc=0;
        if (sscanf(arg, "%63s %255s%n", pname, ppath, &nc) >= 2) {
            const char *extra = arg+nc; while (*extra==' ') extra++;
            if (*extra) {
                char *p=params; size_t left=sizeof(params)-1;
                while (*extra&&left) {
                    if (*extra==' '){if(p>params){*p++=',';left--;}extra++;}
                    else{*p++=*extra++;left--;}
                } *p='\0';
            }
            krepl_panel_load(pname, ppath, params);
        } else {
            repl_println(g_repl, "/ usage: panel load <name> <file.pnl> [key=val ...]");
        }
    } else if (strcmp(sub, "open") == 0) {
        char pname[64]="panel1"; sscanf(arg, "%63s", pname);
        char *picked = repl_open_file_dialog(g_repl, "Open Panel", "Panel\t*.pnl\nAll Files\t*");
        if (picked) {
            krepl_panel_load(pname, picked, NULL);
            repl_free_string(picked);
        }
    } else if (strcmp(sub, "reload") == 0) {
        if (panel_registry_reload(arg)==0) repl_printf(g_repl,"/ panel '%s' reloaded\n",arg);
        else repl_printf(g_repl,"/ panel reload failed: '%s'\n",arg);
    } else if (strcmp(sub, "show") == 0) {
        panel_registry_show(arg); repl_printf(g_repl,"/ panel '%s' shown\n",arg);
    } else if (strcmp(sub, "hide") == 0) {
        panel_registry_hide(arg); repl_printf(g_repl,"/ panel '%s' hidden\n",arg);
    } else if (strcmp(sub, "list") == 0) {
        repl_println(g_repl,"/ panels:");
        panel_registry_list(panel_list_cb, NULL);
    } else if (strcmp(sub, "set") == 0) {
        char pname[64]={0},ctrl[64]={0},val[128]={0}; int fire=0;
        if (sscanf(arg,"%63s %63s %127s %d",pname,ctrl,val,&fire)>=3) {
            if (panel_registry_set_value(pname,ctrl,val,fire)==0)
                repl_printf(g_repl,"/ %s.%s = %s\n",pname,ctrl,val);
            else repl_printf(g_repl,"/ set failed: '%s.%s' not found\n",pname,ctrl);
        } else repl_println(g_repl,"/ usage: panel set <name> <ctrl> <value> [fire=0|1]");
    } else if (strcmp(sub, "get") == 0) {
        char pname[64]={0},ctrl[64]={0},buf[128]={0};
        if (sscanf(arg,"%63s %63s",pname,ctrl)==2) {
            if (panel_registry_get_value(pname,ctrl,buf,sizeof(buf))==0)
                repl_printf(g_repl,"/ %s.%s = %s\n",pname,ctrl,buf);
            else repl_printf(g_repl,"/ get failed: '%s.%s' not found\n",pname,ctrl);
        } else repl_println(g_repl,"/ usage: panel get <name> <ctrl>");
    } else if (strcmp(sub, "dump") == 0) {
        char pname[64]={0}; sscanf(arg,"%63s",pname);
        repl_printf(g_repl,"/ controls for '%s':\n",pname);
        if (panel_registry_enum_values(pname,panel_ctrl_cb,NULL)!=0)
            repl_printf(g_repl,"/ panel '%s' not found\n",pname);
    } else {
        repl_printf(g_repl,"/ unknown panel subcommand: %s\n",sub);
    }
}

/* ---- backslash command dispatcher ---------------------------------------- */

static void handle_backslash_cmd(const char *line) {
    char cmd = line[1];

    if (cmd == 't') {
        g_show = !g_show;
        repl_printf(g_repl,"/ show %s\n",g_show?"on":"off");

    } else if (cmd == '?') {
        repl_println(g_repl,
            "\\p [s] VAR [+/-semi[:cents]] [gain]   play variable\n"
            "\\q                                    stop all voices\n"
            "\\x                                    voice status\n"
            "\\V N                                  set polyphony (1-64)\n"
            "\\A N VAR [s]                          assign var to slot 1-16\n"
            "\\P N [+/-semi[:cents]] [gain]         play slot N\n"
            "\\b                                    list all 16 slots\n"
            "\\k N                                  clear slot N\n"
            "\\s [s] VAR                            save WAV\n"
            "\\c VAR                                export C header\n"
            "\\v [VAR]                              list/inspect variables\n"
            "\\W VAR                                waveform window\n"
            "\\Z VAR                                spectrogram window\n"
            "\\l [FILE]                             load .ks (picker if no arg)\n"
            "\\w MS                                 wait milliseconds\n"
            "\\t                                    toggle scope-after-eval\n"
            "\\?                                    this help\n"
            "\npanel load <name> <file.pnl> [key=val ...]\n"
            "panel open [name]  |  panel reload|show|hide <name>\n"
            "panel list  |  panel set|get|dump ...\n"
            "\nPanel @-blocks (evaluated at panel load time):\n"
            "  @ksynth          <- multi-line ksynth/krepl init code\n"
            "  A=sin(...)       <- any krepl expression or \\command\n"
            "  \\A 1 A\n"
            "  @end\n"
            "  @skred           <- skred code (skipped in krepl)\n"
            "  @end\n"
            "\npitch: +7=fifth  -12=octave down  +3:50=3.5 semitones\n"
            "gain:  0.5=-6dB   2.0=+6dB   1.0=unity (default)");

    } else if (cmd == 'V') {
        /* \V N  — set polyphony at runtime */
        const char *arg = line + 2; while (*arg == ' ') arg++;
        int n = atoi(arg);
        if (n < 1) { repl_println(g_repl, "/ \\V: polyphony must be >= 1"); return; }
        if (n > MAX_VOICES_CAP) {
            repl_printf(g_repl, "/ \\V: clamped to %d\n", MAX_VOICES_CAP);
            n = MAX_VOICES_CAP;
        }
        int old = g_n_voices;
        if (set_polyphony(n) == 0)
            repl_printf(g_repl, "/ polyphony: %d -> %d voices\n", old, g_n_voices);
        else
            repl_println(g_repl, "/ \\V: allocation failed");

    } else if (cmd == 'p') {
        const char *arg = line + 2; while (*arg==' ') arg++;
        int stereo = 0;
        if (*arg=='s' && (arg[1]==' '||arg[1]=='\0')) { stereo=1; arg++; }
        char v_name = get_var_name(arg);
        if (!v_name) { repl_println(g_repl,"/ \\p: no variable"); return; }
        K v = g_ctx->vars[v_name-'A'];
        if (!v) { repl_printf(g_repl,"/ %c is empty\n",v_name); return; }
        while (*arg&&!isspace((unsigned char)*arg)) arg++;
        double rate; float gain; parse_pitch_gain(arg, &rate, &gain);
        v->r++;
        voice_start(v, stereo, rate, gain);

    } else if (cmd == 'q') {
        for (int i=0;i<g_n_voices;i++) {
            if (g_voices[i].buffer) k_free(g_ctx,g_voices[i].buffer);
            g_voices[i].buffer=NULL; g_voices[i].active=0;
        }
        repl_println(g_repl,"/ stopped all voices");

    } else if (cmd == 'x') {
        repl_printf(g_repl,"/ polyphony: %d voices\n",g_n_voices);
        int any=0;
        for (int i=0;i<g_n_voices;i++) {
            if (g_voices[i].active&&g_voices[i].buffer) {
                int nf=g_voices[i].n_frames;
                int pct=(int)(g_voices[i].phase/nf*100.0);
                double sf=log2(g_voices[i].rate)*12.0;
                int semi=(int)round(sf), centi=(int)round((sf-semi)*100.0);
                repl_printf(g_repl,"  [%d] %s  %d/%d (%d%%)  pitch %+d:%+d  gain %.2f\n",
                    i,g_voices[i].stereo?"stereo":"mono  ",
                    (int)g_voices[i].phase,nf,pct,semi,centi,g_voices[i].gain);
                any=1;
            }
        }
        if (!any) repl_println(g_repl,"  (none active)");

    } else if (cmd == 'A') {
        const char *arg=line+2; while(*arg==' ')arg++;
        int n=atoi(arg);
        if (n<1||n>NUM_SLOTS){repl_printf(g_repl,"/ \\A: slot 1-%d\n",NUM_SLOTS);return;}
        while(*arg&&!isspace((unsigned char)*arg))arg++; while(*arg==' ')arg++;
        int stereo=0; if(*arg=='s'){stereo=1;arg++;while(*arg==' ')arg++;}
        char v_name=get_var_name(arg);
        if (!v_name){repl_println(g_repl,"/ \\A: no variable");return;}
        K v=g_ctx->vars[v_name-'A'];
        if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
        Slot *sl=&g_slots[n-1]; free(sl->buf);
        sl->buf=k_to_float(v); sl->len=v->n; sl->stereo=stereo;
        snprintf(sl->label,sizeof(sl->label),"%c",v_name);
        repl_printf(g_repl,"/ slot %2d <- %c [%d samples / %.1fms]%s\n",
            n,v_name,v->n,(double)v->n/44100.0*1000.0,stereo?" (stereo)":"");

    } else if (cmd == 'P') {
        const char *arg=line+2; while(*arg==' ')arg++;
        int n=atoi(arg);
        if (n<1||n>NUM_SLOTS){repl_printf(g_repl,"/ \\P: slot 1-%d\n",NUM_SLOTS);return;}
        Slot *sl=&g_slots[n-1];
        if (!sl->buf){repl_printf(g_repl,"/ slot %d is empty\n",n);return;}
        while(*arg&&!isspace((unsigned char)*arg))arg++;
        double rate; float gain; parse_pitch_gain(arg, &rate, &gain);
        K tmp=k_new_perm(g_ctx,sl->len);
        if (!tmp){repl_println(g_repl,"/ out of memory");return;}
        for (int i=0;i<sl->len;i++) tmp->f[i]=(double)sl->buf[i];
        repl_printf(g_repl,"/ slot %d -> ",n);
        voice_start(tmp, sl->stereo, rate, gain);

    } else if (cmd == 'b') {
        repl_println(g_repl,"/ slots:");
        for (int i=0;i<NUM_SLOTS;i++) {
            Slot *sl=&g_slots[i];
            if (sl->buf)
                repl_printf(g_repl,"  \x1b[36m[%2d]\x1b[0m %-8s %6d samples / %7.1fms%s\n",
                    i+1,sl->label,sl->len,(double)sl->len/44100.0*1000.0,sl->stereo?" stereo":"");
            else
                repl_printf(g_repl,"  \x1b[38;5;240m[%2d] ---\x1b[0m\n",i+1);
        }

    } else if (cmd == 'k') {
        const char *arg=line+2; while(*arg==' ')arg++;
        int n=atoi(arg);
        if (n<1||n>NUM_SLOTS){repl_printf(g_repl,"/ \\k: slot 1-%d\n",NUM_SLOTS);return;}
        Slot *sl=&g_slots[n-1]; free(sl->buf);
        sl->buf=NULL; sl->len=0; sl->label[0]='\0';
        repl_printf(g_repl,"/ slot %d cleared\n",n);

    } else if (cmd == 's') {
        const char *arg=line+2; while(*arg==' ')arg++;
        int stereo=0; if(*arg=='s'){stereo=1;arg++;}
        char v_name=get_var_name(arg);
        if (!v_name){repl_println(g_repl,"/ \\s: no variable");return;}
        K v=g_ctx->vars[v_name-'A'];
        if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
        struct timeval tv; gettimeofday(&tv,NULL);
        char name[64]; snprintf(name,sizeof(name),"%c-%ld.wav",v_name,(long)tv.tv_sec);
        ma_uint32 chans=stereo?2:1;
        write_wav(name,v->f,stereo?(ma_uint64)(v->n/2):(ma_uint64)v->n,chans,44100);

    } else if (cmd == 'c') {
        char v_name=get_var_name(line+2);
        if (!v_name){repl_println(g_repl,"/ \\c: no variable");return;}
        K v=g_ctx->vars[v_name-'A'];
        if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
        char fname[64]; struct timeval tv; gettimeofday(&tv,NULL);
        snprintf(fname,sizeof(fname),"%c-%ld.h",v_name,(long)tv.tv_sec);
        FILE *f=fopen(fname,"w");
        if (!f){repl_printf(g_repl,"/ cannot write %s\n",fname);return;}
        fprintf(f,"float %c[%d] = {\n",v_name,v->n);
        int col=0;
        for (int i=0;i<v->n;i++){fprintf(f,"%g, ",v->f[i]);if(++col>=64){fprintf(f,"\n");col=0;}}
        if (col) fprintf(f,"\n"); fprintf(f,"};\n"); fclose(f);
        repl_printf(g_repl,"/ wrote %s\n",fname);

    } else if (cmd == 'v') {
        if (line[2]=='\0'||line[2]==' ') {
            int any=0;
            for (int c='A';c<='Z';c++) {
                K v=g_ctx->vars[c-'A'];
                if (v){repl_printf(g_repl,"\x1b[36m%c\x1b[0m [%d] / %.2fms\n",
                    c,v->n,(double)v->n/44100.0*1000.0);any=1;}
            }
            if (!any) repl_println(g_repl,"/ no variables set");
        } else {
            char v_name=get_var_name(line+2); if (v_name) show_var(v_name);
        }

    } else if (cmd == 'W') {
        char v_name=get_var_name(line+2);
        if (!v_name){repl_println(g_repl,"/ \\W: no variable");return;}
        K v=g_ctx->vars[v_name-'A'];
        if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
        float *buf=k_to_float(v); if (!buf) return;
        char wtitle[32]; snprintf(wtitle,sizeof(wtitle),"waveform: %c",v_name);
        bitmap_win_t *bw=bitmap_win_get(wtitle);
        bitmap_win_set_waveform_ex(bw,buf,v->n,1,0,800,300,wtitle,-1,-1,44100.0f);
        bitmap_win_show(bw); free(buf);
        repl_printf(g_repl,"/ waveform window: %c [%d samples]\n",v_name,v->n);

    } else if (cmd == 'Z') {
        char v_name=get_var_name(line+2);
        if (!v_name){repl_println(g_repl,"/ \\Z: no variable");return;}
        K v=g_ctx->vars[v_name-'A'];
        if (!v){repl_printf(g_repl,"/ %c is empty\n",v_name);return;}
        float *buf=k_to_float(v); if (!buf) return;
        char ztitle[32]; snprintf(ztitle,sizeof(ztitle),"spectrogram: %c",v_name);
        bitmap_win_t *bw=bitmap_win_get(ztitle);
        bitmap_win_set_spectrogram_labeled_ex(bw,buf,v->n,1,0,800,300,ztitle,44100.0f);
        bitmap_win_show(bw); free(buf);
        repl_printf(g_repl,"/ spectrogram window: %c [%d samples]\n",v_name,v->n);

    } else if (cmd == 'l') {
        const char *fn=line+2; while(*fn==' ')fn++;
        if (*fn=='\0') {
            char *picked=repl_open_file_dialog(g_repl,"Load K-Synth Script","K-Synth\t*.ks\nAll Files\t*");
            if (picked){handle_ks_file(picked);repl_free_string(picked);}
        } else {
            handle_ks_file(fn);
        }

    } else if (cmd == 'w') {
        int ms=atoi(line+2); if (ms>0) usleep((useconds_t)ms*1000);

    } else {
        repl_printf(g_repl,"/ unknown command: \\%c  (\\? for help)\n",cmd);
    }
}

/* ---- K-Synth evaluator --------------------------------------------------- */

static void eval_k(const char *code) {
    if (!code||!*code) return;
    char buf[4096]; strncpy(buf,code,sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    int depth=0;
    for (char *p=buf;*p;p++){
        if(*p=='{')depth++; else if(*p=='}'&&depth>0)depth--;
        else if(*p=='/'&&depth==0){*p='\0';break;}
    }
    K r=ks_eval(g_ctx,buf,strlen(buf));
    if (g_ctx->last_status!=KS_OK) {
        repl_printf(g_repl,"\x1b[31m/ error %d: %s\x1b[0m\n",
            g_ctx->last_status,ks_strerror(g_ctx->last_status));
    } else {
        g_ctx->gas_used=0;
        if (r&&r->n>0) {
            if (r->n==1) {
                repl_printf(g_repl,"\x1b[38;5;51m%.6f\x1b[0m\n",r->f[0]);
            } else {
                repl_printf(g_repl,"\x1b[38;5;51m[%d] (",r->n);
                int lim=r->n<8?r->n:8;
                for (int i=0;i<lim;i++)
                    repl_printf(g_repl,"%.4f%s",r->f[i],i==lim-1?"":" ");
                repl_println(g_repl,r->n>8?" ...)\x1b[0m":")\x1b[0m");
                if (g_show) print_scope_to_repl(r->f,r->n,128,32);
            }
        }
    }
    k_free(g_ctx,r);
}

/* ---- line dispatch ------------------------------------------------------- */

static void handle_line_single(const char *line) {
    while (*line==' ') line++;
    if (line[0]=='\0'||line[0]=='/') return;
    if (line[0]=='\\') { handle_backslash_cmd(line); return; }
    if (strncmp(line,"panel",5)==0&&(line[5]==' '||line[5]=='\0')) {
        const char *rest=line+5; while(*rest==' ')rest++;
        handle_panel_cmd(rest); return;
    }
    eval_k(line);
}

static void handle_line(const char *line) {
    size_t len=strlen(line);
    char *tmp=(char *)malloc(len+1);
    if (!tmp){handle_line_single(line);return;}
    memcpy(tmp,line,len+1);
    char acc[4096]={0}; size_t acc_len=0;
    int pd=0,bd=0,brd=0; char *seg=tmp;
    for (char *p=tmp;;p++) {
        char c=*p;
        if(c=='(')pd++; else if(c==')'&&pd>0)pd--;
        else if(c=='{')bd++; else if(c=='}'&&bd>0)bd--;
        else if(c=='[')brd++; else if(c==']'&&brd>0)brd--;
        int top=(pd==0&&bd==0&&brd==0);
        int bound=(c=='\0'||(top&&c==';'));
        if (!bound) continue;
        char saved=c; *p='\0';
        char *s=seg; while(*s==' ')s++;
        char *e=s+strlen(s); while(e>s&&(e[-1]==' '||e[-1]=='\t'))*--e='\0';
        if (*s) {
            if (s[0]=='\\') {
                if (acc_len){handle_line_single(acc);acc[0]='\0';acc_len=0;}
                handle_line_single(s);
            } else {
                if (acc_len){acc[acc_len++]=';';acc[acc_len]='\0';}
                size_t slen=strlen(s);
                if (acc_len+slen+2<sizeof(acc)){memcpy(acc+acc_len,s,slen+1);acc_len+=slen;}
            }
        }
        if (saved=='\0') break;
        seg=p+1;
    }
    if (acc_len) handle_line_single(acc);
    free(tmp);
}

static void handle_ks_file(const char *path) {
    FILE *f=fopen(path,"r");
    if (!f){repl_printf(g_repl,"/ cannot open: %s\n",path);return;}
    repl_printf(g_repl,"/ loading %s\n",path);
    char buf[4096];
    while (fgets(buf,sizeof(buf),f)){buf[strcspn(buf,"\n")]='\0';handle_line(buf);}
    fclose(f);
}

static void line_fallback(const char *line, void *ud) {
    (void)ud; if (!line||!*line) return; handle_line(line);
}

/* ---- main ---------------------------------------------------------------- */

int main(int argc, char **argv) {
    /* parse --voices N and --check before anything else */
    int start_voices = DEFAULT_VOICES;
    int files_start  = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) {
            printf("krepl 0.6.0\nksynth from github.com/octetta/k-synth\n");
            return 0;
        }
        if (strcmp(argv[i], "--voices") == 0 && i+1 < argc) {
            start_voices = atoi(argv[i+1]);
            if (start_voices < 1) start_voices = 1;
            if (start_voices > MAX_VOICES_CAP) start_voices = MAX_VOICES_CAP;
            /* mark both args as consumed */
            argv[i] = argv[i+1] = NULL;
            i++;
        }
    }

    g_ctx = ks_create(16*1024*1024, 1000000);
    if (!g_ctx){fprintf(stderr,"krepl: ks_create failed\n");return 1;}

    if (set_polyphony(start_voices) != 0) {
        fprintf(stderr,"krepl: failed to allocate voice pool\n"); return 1;
    }

    ma_device_config dcfg = ma_device_config_init(ma_device_type_playback);
    dcfg.playback.format   = ma_format_f32;
    dcfg.playback.channels = 2;
    dcfg.sampleRate        = 44100;
    dcfg.dataCallback      = audio_cb;
    if (ma_device_init(NULL,&dcfg,&g_device)==MA_SUCCESS) {
        ma_device_start(&g_device); g_audio_ok=1;
    }

    g_repl = repl_create("krepl - K-Synth DSP REPL", 900, 640);
    if (!g_repl){fprintf(stderr,"krepl: repl_create failed\n");return 1;}

    repl_set_fallback_handler(g_repl, line_fallback, NULL);
    panel_set_command_handler(panel_to_krepl, NULL);
    panel_set_error_handler(panel_error_cb, NULL);

    repl_println(g_repl, "\x1b[1;36mkrepl 0.6  K-Synth DSP REPL\x1b[0m");
    repl_printf(g_repl,  "/ audio: %s\n",
                g_audio_ok?"ok (44100Hz stereo)":"unavailable");
    repl_printf(g_repl,  "/ polyphony: %d voices  (\\V N to change)\n", g_n_voices);
    repl_println(g_repl, "/ \\? for help  |  panel open to load a panel  |  \\l for a script");

    /* load any .ks files passed on the command line */
    for (int i = files_start; i < argc; i++)
        if (argv[i]) handle_ks_file(argv[i]);

    repl_run(g_repl);
    repl_destroy(g_repl);
    panel_set_command_handler(NULL, NULL);
    panel_set_error_handler(NULL, NULL);

    if (g_audio_ok) {
        for (int i=0;i<g_n_voices;i++)
            if (g_voices[i].buffer) k_free(g_ctx,g_voices[i].buffer);
        ma_device_uninit(&g_device);
    }
    free(g_voices);
    for (int i=0;i<NUM_SLOTS;i++) free(g_slots[i].buf);
    ks_destroy(g_ctx);
    return 0;
}
