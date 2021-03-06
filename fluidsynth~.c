/*
Copyright: Alexandre Torres Porres, based on the work of
 Frank Barknecht, Jonathan Wilkes and Albert Gräf
 Distributed under the GPLv2+, please see LICENSE below.

LICENSE:
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "m_pd.h"
#include <fluidsynth.h>
#include <string.h>
#include <unistd.h>

#define MAXSYSEXSIZE 1024 // Size of sysex data list (excluding the F0 [240] and F7 [247] bytes)

static t_class *fluid_tilde_class;
 
typedef struct _fluid_tilde{
    t_object            x_obj;
    fluid_synth_t      *x_synth;
    fluid_settings_t   *x_settings;
    t_outlet           *x_out_left;
    t_outlet           *x_out_right;
    t_canvas           *x_canvas;
    int                 x_sysex;
    int                 x_count;
    int                 x_ready;
    t_atom              x_at[MAXSYSEXSIZE];
    unsigned char       x_type;
    unsigned char       x_data;
    unsigned char       x_channel;
}t_fluid_tilde;

t_int *fluid_tilde_perform(t_int *w){
    t_fluid_tilde *x = (t_fluid_tilde *)(w[1]);
    t_sample *left = (t_sample *)(w[2]);
    t_sample *right = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    fluid_synth_write_float(x->x_synth, n, left, 0, 1, right, 0, 1);
    return(w+5);
}

static void fluid_tilde_dsp(t_fluid_tilde *x, t_signal **sp){
    dsp_add(fluid_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_n);
}

static void fluid_tilde_free(t_fluid_tilde *x){
    if(x->x_synth)
        delete_fluid_synth(x->x_synth);
    if(x->x_settings)
        delete_fluid_settings(x->x_settings);
}

static void fluid_info(void){
    post(" - [fluidsynth~] uses fluidsynth version: %s ", FLUIDSYNTH_VERSION);
    //            post("[fluidsynth~]: loaded soundfont %s", realname);
    post("\n");
}

static void fluid_note(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 2 || ac == 3){
        int key = atom_getintarg(0, ac, av);
        int vel = atom_getintarg(1, ac, av);
        int chan = ac > 2 ? atom_getintarg(2, ac, av) : 1;
        fluid_synth_noteon(x->x_synth, chan-1, key, vel);
    }
}

static void fluid_program_change(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 1 || ac == 2){
        int prog = atom_getintarg(0, ac, av);
        int chan = ac > 1 ? atom_getintarg(1, ac, av) : 1;
        fluid_synth_program_change(x->x_synth, chan-1, prog);
    }
}

static void fluid_control_change(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 2 || ac == 3){
        int ctrl = atom_getintarg(0, ac, av);
        int val = atom_getintarg(1, ac, av);
        int chan = ac > 2 ? atom_getintarg(2, ac, av) : 1;
        fluid_synth_cc(x->x_synth, chan-1, ctrl, val);
    }
}

static void fluid_pitch_bend(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 1 || ac == 2){
        int val = atom_getintarg(0, ac, av);
        int chan = ac > 1 ? atom_getintarg(1, ac, av) : 1;
        fluid_synth_pitch_bend(x->x_synth, chan-1, val);
    }
}

static void fluid_bank(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 1 || ac == 2){
        int bank = atom_getintarg(0, ac, av);
        int chan = ac > 1 ? atom_getintarg(1, ac, av) : 1;
//        post("bank (%d) chan (%d)", bank, chan);
        fluid_synth_bank_select(x->x_synth, chan-1, bank);
    }
}

static void fluid_gen(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 3){
        int chan, param;
        float value;
        chan = atom_getintarg(0, ac, av);
        param = atom_getintarg(1, ac, av);
        value = atom_getfloatarg(2, ac, av);
        fluid_synth_set_gen(x->x_synth, chan-1, param, value);
        // https://github.com/uliss/pure-data/blob/ceammc/ceammc/ext/src/misc/fluid.cpp#L390
    }
}

static void fluid_aftertouch(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 1 || ac == 2){
        int val = atom_getintarg(0, ac, av);
        int chan = ac > 1 ? atom_getintarg(1, ac, av) : 1;
        fluid_synth_channel_pressure(x->x_synth, chan-1, val);
    }
}

static void fluid_polytouch(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac == 2 || ac == 3){
        int val = atom_getintarg(0, ac, av);
        int key = atom_getintarg(1, ac, av);
        int chan = ac > 2 ? atom_getintarg(2, ac, av) : 1;
        fluid_synth_key_pressure(x->x_synth, chan-1, key, val);
    }
}

static void fluid_sysex(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL)
        return;
    if(ac > 0){
        char buf[MAXSYSEXSIZE];
        int len = 0;
        while(len < MAXSYSEXSIZE && len < ac){
            buf[len] = atom_getintarg(len, ac, av);
            len++;
        }
        // TODO: In order to handle bulk dump requests in the future, we will
        // have to pick up fluidsynth's response here and output that to a
        // control outlet (which doesn't exist at present).
        fluid_synth_sysex(x->x_synth, buf, len, NULL, NULL, NULL, 0);
    }
}

static void fluid_float(t_fluid_tilde *x, t_float f){
    if(f >= 0 && f <= 255){
        unsigned char val = (unsigned char)f;
        if(val > 127){ // not a data type
            if(val == 0xF0){ // start of sysex
                x->x_sysex = 1;
                x->x_count = 0;
            }
            else if(val == 0xF7){ // end of sysex
                fluid_sysex(x, &s_list, x->x_count, x->x_at);
                x->x_sysex = x->x_count = 0;
            }
            else{
                x->x_type = val & 0xF0; // get type
                x->x_channel = (val & 0x0F) + 1; // get channel
                x->x_ready = (x->x_type == 0xC0 || x->x_type == 0xD0); // ready if program or touch
            }
        }
        else if(x->x_sysex){
            SETFLOAT(&x->x_at[x->x_count], (t_float)val);
            x->x_count++;
        }
        else{
            if(x->x_ready){
                switch(x->x_type){
                    case 0x80: // group 128-143 (NOTE OFF)
                    SETFLOAT(&x->x_at[0], (t_float)x->x_data); // key
                    SETFLOAT(&x->x_at[1], 0); // make it note on with velocity 0
                    SETFLOAT(&x->x_at[2], (t_float)x->x_channel);
                    fluid_note(x, &s_list, 3, x->x_at);
                    break;
                case 0x90: // group 144-159 (NOTE ON)
                    SETFLOAT(&x->x_at[0], (t_float)x->x_data); // key
                    SETFLOAT(&x->x_at[1], val); // velocity
                    SETFLOAT(&x->x_at[2], (t_float)x->x_channel);
                    fluid_note(x, &s_list, 3, x->x_at);
                    break;
                case 0xA0: // group 160-175 (POLYPHONIC AFTERTOUCH)
                    SETFLOAT(&x->x_at[0], val); // aftertouch pressure value
                    SETFLOAT(&x->x_at[1], (t_float)x->x_data); // key
                    SETFLOAT(&x->x_at[2], (t_float)x->x_channel);
                    fluid_polytouch(x, &s_list, 3, x->x_at);
                    break;
                case 0xB0: // group 176-191 (CONTROL CHANGE)
                    SETFLOAT(&x->x_at[0], val); // control value
                    SETFLOAT(&x->x_at[1], (t_float)x->x_data); // control number
                    SETFLOAT(&x->x_at[2], (t_float)x->x_channel);
                    fluid_control_change(x, &s_list, 3, x->x_at);
                    break;
                case 0xC0: // group 192-207 (PROGRAM CHANGE)
                    SETFLOAT(&x->x_at[0], val); // control value
                    SETFLOAT(&x->x_at[1], (t_float)x->x_channel);
                    fluid_program_change(x, &s_list, 2, x->x_at);
                    break;
                case 0xD0: // group 208-223 (AFTERTOUCH)
                    SETFLOAT(&x->x_at[0], val); // control value
                    SETFLOAT(&x->x_at[1], (t_float)x->x_channel);
                    fluid_aftertouch(x, &s_list, 2, x->x_at);
                    break;
                case 0xE0: // group 224-239 (PITCH BEND)
                    SETFLOAT(&x->x_at[0], (val << 7) + x->x_data);
                    SETFLOAT(&x->x_at[1], x->x_channel);
                    fluid_pitch_bend(x, &s_list, 2, x->x_at);
                    break;
                default:
                    break;
                }
                x->x_type = x->x_ready = 0; // clear
            }
            else{ // not ready, get data and make it ready
                x->x_data = val;
                x->x_ready = 1;
            }
        }
    }
    else
        x->x_type = x->x_ready = 0; // clear
}

static void fluid_load(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth == NULL){
        pd_error(x, "[fluidsynth~]: no fluidsynth");
        return;
    }
    if(ac >= 1 && av->a_type == A_SYMBOL){
        const char* filename = atom_getsymbolarg(0, ac, av)->s_name;
        const char* ext = strrchr(filename, '.');
        char realdir[MAXPDSTRING], *realname = NULL;
        int fd;
        if(ext && !strchr(ext, '/')){ // extension already supplied, no default extension
            ext = "";
            fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
            if(fd < 0){
              pd_error(x, "[fluidsynth~]: can't find soundfont %s", filename);
              return;
            }
        }
        else{
            ext = ".sf2"; // let's try sf2
            fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
            if(fd < 0){ // failed
                ext = ".sf3"; // let's try sf3 then
                fd = canvas_open(x->x_canvas, filename, ext, realdir, &realname, MAXPDSTRING, 0);
                if(fd < 0){ // also failed
                    pd_error(x, "[fluidsynth~]: can't find soundfont %s", filename);
                   return;
                }
            }
        }
        // Save the current working directory.
        char buf[MAXPDSTRING], *cwd = getcwd(buf, MAXPDSTRING);
        sys_close(fd);
        chdir(realdir);
        if(fluid_synth_sfload(x->x_synth, realname, 0) >= 0)
            fluid_synth_program_reset(x->x_synth);
        cwd && chdir(cwd); // Restore the working directory.
    }
}

/*static void fluid_init(t_fluid_tilde *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(x->x_synth)
        delete_fluid_synth(x->x_synth);
    if(x->x_settings)
        delete_fluid_settings(x->x_settings);
}*/

static void *fluid_tilde_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_fluid_tilde *x = (t_fluid_tilde *)pd_new(fluid_tilde_class);
    x->x_synth = NULL;
    x->x_settings = NULL;
    x->x_sysex = x->x_ready = x->x_count = 0;
    x->x_data = x->x_channel = x->x_type = 0;
    x->x_out_left = outlet_new(&x->x_obj, &s_signal);
    x->x_out_right = outlet_new(&x->x_obj, &s_signal);
    x->x_canvas = canvas_getcurrent();
    x->x_settings = new_fluid_settings();
    if(x->x_settings == NULL){
        pd_error(x, "[fluidsynth~]: couldn't create synth settings\n");
        goto end;
    }
    else{ // load settings:
        fluid_settings_setnum(x->x_settings, "synth.midi-channels", 16);
        fluid_settings_setnum(x->x_settings, "synth.polyphony", 256);
        fluid_settings_setnum(x->x_settings, "synth.gain", 0.600000);
        fluid_settings_setnum(x->x_settings, "synth.sample-rate", sys_getsr());
        fluid_settings_setstr(x->x_settings, "synth.chorus.active", "no");
        fluid_settings_setstr(x->x_settings, "synth.reverb.active", "no");
        fluid_settings_setstr(x->x_settings, "synth.ladspa.active", "no");
        x->x_synth = new_fluid_synth(x->x_settings); // Create fluidsynth instance:
        if(x->x_synth == NULL){
            pd_error(x, "[fluidsynth~]: couldn't create synth");
            goto end;
        }
        fluid_load(x, gensym("load"), ac, av); // try to load argument as soundfont
    }
end:
    return(void *)x;
}
 
void fluidsynth_tilde_setup(void){
    fluid_tilde_class = class_new(gensym("fluidsynth~"), (t_newmethod)fluid_tilde_new,
        (t_method)fluid_tilde_free, sizeof(t_fluid_tilde), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(fluid_tilde_class, (t_method)fluid_float); // raw midi input
    class_addmethod(fluid_tilde_class, (t_method)fluid_load, gensym("load"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_gen, gensym("gen"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_bank, gensym("bank"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_note, gensym("note"), A_GIMME, 0);
    class_addlist(fluid_tilde_class, (t_method)fluid_note); // same as note
    class_addmethod(fluid_tilde_class, (t_method)fluid_control_change, gensym("ctl"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_program_change, gensym("pgm"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_polytouch, gensym("polytouch"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_aftertouch, gensym("touch"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_pitch_bend, gensym("bend"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_sysex, gensym("sysex"), A_GIMME, 0);
    class_addmethod(fluid_tilde_class, (t_method)fluid_info, gensym("info"), 0);
}

//  fluid_synth_system_reset(synth_); // panic
