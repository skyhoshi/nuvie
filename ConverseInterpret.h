#ifndef __ConverseInterpret_h__
#define __ConverseInterpret_h__
/* Created by Joseph Applegate
 * Copyright (C) 2003 The Nuvie Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
#include "Converse.h"

using std::string;
using std::stack;
using std::vector;

/* Control and value opcodes for op() & evop() (U6) */
#define U6OP_GT         0x81
#define U6OP_GT2        0x82
#define U6OP_LT         0x83
#define U6OP_LT2        0x84
#define U6OP_NE         0x85
#define U6OP_EQ         0x86
#define U6OP_ADD        0x90
#define U6OP_SUB        0x91
#define U6OP_MUL        0x92
#define U6OP_LOR        0x94
#define U6OP_LAND       0x95
#define U6OP_CANCARRY   0x9a
#define U6OP_WEIGHT     0x9b
#define U6OP_RAND       0xa0
#define U6OP_EVAL       0xa7
#define U6OP_FLAG       0xab
#define U6OP_VAR        0xb2
#define U6OP_SVAR       0xb3
#define U6OP_DATA       0xb4
#define U6OP_OBJCOUNT   0xbb
#define U6OP_INPARTY    0xc6
#define U6OP_OBJINPARTY 0xc7
#define U6OP_NPCNEARBY  0xd7
#define U6OP_WOUNDED    0xda
#define U6OP_POISONED   0xdc
#define U6OP_NPC        0xdd
#define U6OP_EXP        0xe0 // FIXME: stat ops need to add to value
#define U6OP_LVL        0xe1
#define U6OP_STR        0xe2
#define U6OP_INT        0xe3
#define U6OP_DEX        0xe4

#define U6OP_IF        0xa1
#define U6OP_ENDIF     0xa2
#define U6OP_ELSE      0xa3
#define U6OP_SETF      0xa4
#define U6OP_CLEARF    0xa5
#define U6OP_DECL      0xa6
#define U6OP_ASSIGN    0xa8
#define U6OP_JUMP      0xb0
#define U6OP_BYE       0xb6
#define U6OP_NEW       0xb9
#define U6OP_DELETE    0xba
#define U6OP_INVENTORY 0xbe
#define U6OP_PORTRAIT  0xbf
#define U6OP_GIVE      0xc9
#define U6OP_ADDKARMA  0xc4
#define U6OP_SUBKARMA  0xc5
#define U6OP_WAIT      0xcb
#define U6OP_WORKTYPE  0xcd
#define U6OP_SETNAME   0xd8
#define U6OP_HEAL      0xd9
#define U6OP_CURE      0xdb
#define U6OP_ENDANSWER 0xee
#define U6OP_KEYWORDS  0xef
#define U6OP_SIDENT    0xff
#define U6OP_SLOOK     0xf1
#define U6OP_SCONVERSE 0xf2
#define U6OP_SPREFIX   0xf3
#define U6OP_ANSWER    0xf6
#define U6OP_ASK       0xf7
#define U6OP_ASKC      0xf8
#define U6OP_INPUT     0xfb
#define U6OP_INPUTNUM  0xfc

// FIXME: This class is for U6 conversations, but it should be generalized.
/* Script is executed as it is stepped through byte-by-byte, and can have
 * text, data, and control codes. Flow is controlled by run-level stack.
 */
class ConverseInterpret
{
    Converse *converse; // to get data from container

    bool is_waiting; // return control to Converse, paused waiting for something
    bool stopped; // conversation will end, after control returns to Converse

    struct in_val_s
    {
        converse_value v; // data
        uint8 d; // data-size or 0x00
    };
    vector<struct in_val_s> *in; // control values (input/instruction)
    uint32 in_start;
    string *text; // input text from script
    const char **rstrings; // string value(s) returned by op
    uint32 rstring_count;
    char *ystring; // modified by SETNAME, accessed with "$Y"

    const char *get_rstr(uint32 sn) { return((sn < rstring_count) ? rstrings[sn] : ""); }
    const char *get_ystr()          { return(ystring ? ystring : ""); }
    void set_rstr(uint32 sn, const char *s);
    void set_ystr(const char *s)    { free(ystring); ystring = strdup(s); }

    bool answer; // response has been triggered by conversation input?
    uint8 decl_v; // declared/initialized variable number
    uint8 decl_t; // declared variable type: 0x00=none,0xb2=int,0xb3=string
    void let(uint8 v, uint8 t) { decl_v = v; decl_t = t; }
    void let()                 { decl_v = decl_t = 0x00; }

    struct convi_frame_s
    {
        uint32 start;
        converse_value start_c; // enter on c
        bool run; // run(true) or skip(false) instructions
        converse_value break_c; // will toggle run setting
    };
    stack<struct convi_frame_s *> *b_frame;
    void enter(converse_value c);
    void leave();
    void leave_all() { while(b_frame && !b_frame->empty()) leave(); }
    struct convi_frame_s *top_frame()
     { return((b_frame && !b_frame->empty()) ? b_frame->top() : NULL); }
    void do_frame(converse_value c);

    void set_break(converse_value c) {if(top_frame()) top_frame()->break_c = c;}
    converse_value get_break()       {return(top_frame()?top_frame()->break_c:0x00);}
    void clear_break()               { set_break(0x00); }
    void set_run(bool r) { if(top_frame()) top_frame()->run = r; }
    bool get_run()       { return(top_frame() ? top_frame()->run : true); }

    struct converse_db_s // ONE data item from a converse script db
    {
        uint8 type; // 0=s 1=i
        char *s;
        uint8 i;
    };

public:
    ConverseInterpret(Converse *owner);
    ~ConverseInterpret();

    bool waiting() { return(is_waiting); }
    void wait()    { is_waiting = true; }
    void unwait()  { is_waiting = false; }
    void stop()    { stopped = true; wait(); }
    bool end()     { return(stopped); }

    void step();

protected:
    /* collecting from script */
    void collect_input();
    struct in_val_s read_value();
    void eval(uint32 vi = 0);

    void add_val(converse_value c, uint8 d = 0);
    void add_text(unsigned char c = 0);

    /* manipulating collected input */
    uint32 val_count()     { return(in ? in->size() : 0); }
    converse_value get_val(uint32 vi);
    uint8 get_val_size(uint32 vi);
    converse_value pop_val();
    uint8 pop_val_size();
    const char *get_text() { return(text ? text->c_str() : NULL); }
    void flush() {if(in) in->resize(0); in_start = 0; if(text) text->resize(0);}

    /* operating on input */
    void exec();
    void do_ctrl();
    void do_text();
    converse_value pop_arg(stack<converse_value> &vs);
    void evop(stack<converse_value> &i);
    void op(stack<converse_value> &i);

public:
    /* Return `n' as-is or convert to number of NPC from Converse. */
//    uint8 npc_num(uint32 n) { return((n != 0xeb) ? n : converse->npc_num); }
    uint8 npc_num(uint32 n);
    bool check_keywords(const char *keystr, const char *instr);
    bool var_input() { return(decl_t != 0x00); }
    void assign_input(); // set declared variable to Converse input
    struct converse_db_s *get_db(uint32 loc, uint32 i);

    /* value tests */
    bool is_print(converse_value check)
     { return( ((check == 0x0a) || (check >= 0x20 && check <=0x7a)) ); }
    bool is_ctrl(converse_value code)
     { return((code >= 0xa1 && !is_valop(code) && !is_datasize(code))); }
    bool is_datasize(converse_value check)
     { return((check == 0xd3 || check == 0xd2 || check == 0xd4)); }
    bool is_valop(converse_value check)
    {
        return( ((check == 0x81) || (check == 0x82) || (check == 0x83)
                 || (check == 0x84) || (check == 0x85) || (check == 0x86)
                 || (check == 0x90) || (check == 0x91) || (check == 0x92)
                 || (check == 0x94) || (check == 0x95) || (check == 0x9a)
                 || (check == 0x9b) || (check == 0xa0) || (check == 0xa7)
                 || (check == 0xab) || (check == 0xb2) || (check == 0xb3)
                 || (check == 0xb4) || (check == 0xbb) || (check == 0xc6)
                 || (check == 0xc7) || (check == 0xd7) || (check == 0xda)
                 || (check == 0xdc) || (check == 0xdd) || (check == 0xe0)
                 || (check == 0xe1) || (check == 0xe2) || (check == 0xe3)
                 || (check == 0xe4)) );
    }

};

#endif /* __ConverseInterpret_h__ */
