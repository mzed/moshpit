#include "ext.h"
#include "ext_obex.h"
#include "jpatcher_api.h"
#include "jgraphics.h"

#define ARRAY_SIZE 1024
#define NMAX 50
#define RADIUS 1.0
#define TWO_R 2.0
#define FR 4.0
#define VHAPPY 1.0
#define DAMP 1.0
#define GDT 0.1
#define FRAC 0.15

typedef struct _moshpit
{
	t_jbox u_box;						// header for UI objects
	void *m_out;						// outlet pointer
	long u_state;						// state (1 or 0)
	char u_mouseover;					// is mouse over the object
	char u_mousedowninside;				// is mouse down within the object
	char u_trackmouse;					// if non-zero, track mouse when button not down
	t_jrgba u_outline;
	t_jrgba u_background;
	t_jrgba u_grey;
    t_jrgba u_red;
    
    void *m_clock;
    
    //moshpit globals
    long numMoshers;
    double mpX[ARRAY_SIZE];
    double mpY[ARRAY_SIZE];
    double vx[ARRAY_SIZE];
    double vy[ARRAY_SIZE];
    double fx[ARRAY_SIZE];
    double fy[ARRAY_SIZE];
    double col[ARRAY_SIZE];
    double r[ARRAY_SIZE];
    long type[ARRAY_SIZE];
    double colavg;
    long frameskip;
    long framerate;
    
    //neighborlist
    long lx;
    long ly;
    long size[2];
    long cells[4096];
    long count[ARRAY_SIZE];
    
    //things we can change
    long pbc[2];
    long epsilon;
    double flock;
    double noise;
    char showforce;
    
} t_moshpit;

void *moshpit_new(t_symbol *s, long argc, t_atom *argv);
void moshpit_free(t_moshpit *x);
void moshpit_assist(t_moshpit *x, void *b, long m, long a, char *s);
void moshpit_paint(t_moshpit *x, t_object *patcherview);
void moshpit_getdrawparams(t_moshpit *x, t_object *patcherview, t_jboxdrawparams *params);
void moshpit_mousedown(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void moshpit_mousedrag(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void moshpit_mouseup(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void moshpit_mouseenter(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void moshpit_mouseleave(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void moshpit_mousemove(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers);
void init_sidelength(t_moshpit *x, long L);
void init_circle (t_moshpit *x);
void moshpit_bang(t_moshpit *x);
void moshpit_int(t_moshpit *x, long n);

static t_class *s_moshpit_class;

void ext_main(void *r)
{
	t_class *c;

	c = class_new("moshpit", (method)moshpit_new, (method)moshpit_free, sizeof(t_moshpit), 0L, A_GIMME, 0);

	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;
	jbox_initclass(c, JBOX_FIXWIDTH | JBOX_COLOR);

	class_addmethod(c, (method)moshpit_paint,		"paint",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mousedown,	"mousedown",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mousedrag,	"mousedrag",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mouseup,		"mouseup",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mouseenter,	"mouseenter",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mouseleave,	"mouseleave",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_mousemove,	"mousemove",	A_CANT, 0);
	class_addmethod(c, (method)moshpit_assist,		"assist",	A_CANT, 0);

	// messages for state setting / retrieval

	class_addmethod(c, (method)moshpit_int,			"int",	A_LONG, 0);
	class_addmethod(c, (method)moshpit_bang,			"bang", 0);

	// attributes

	CLASS_ATTR_CHAR(c, "trackmouse", 0, t_moshpit, u_trackmouse);
	CLASS_ATTR_STYLE_LABEL(c, "trackmouse", 0, "onoff", "Track Mouse");
	CLASS_ATTR_SAVE(c, "trackmouse", 0);
	CLASS_ATTR_CATEGORY(c, "trackmouse", 0, "Behavior");
    
    CLASS_ATTR_LONG(c, "numMoshers", 0, t_moshpit, numMoshers);
    CLASS_ATTR_STYLE_LABEL(c, "numMoshers", 0, 0, "Number of Moshers");
    //CLASS_ATTR_SAVE(c, "numMoshers", 0);
    CLASS_ATTR_CATEGORY(c, "numMoshers", 0, "moshpit");
    
    CLASS_ATTR_DOUBLE(c, "noise", 0, t_moshpit, noise);
    CLASS_ATTR_STYLE_LABEL(c, "noise", 0, 0, "noise");
    //CLASS_ATTR_SAVE(c, "noise", 0);
    CLASS_ATTR_CATEGORY(c, "noise", 0, "moshpit");
    
    CLASS_ATTR_DOUBLE(c, "flock", 0, t_moshpit, flock);
    CLASS_ATTR_STYLE_LABEL(c, "flock", 0, 0, "flock");
    //CLASS_ATTR_SAVE(c, "flock", 0);
    CLASS_ATTR_CATEGORY(c, "flock", 0, "moshpit");
    
    CLASS_ATTR_LONG(c, "frameskip", 0, t_moshpit, frameskip);
    CLASS_ATTR_STYLE_LABEL(c, "frameskip", 0, 0, "frames between renders");
    //CLASS_ATTR_SAVE(c, "flock", 0);
    CLASS_ATTR_CATEGORY(c, "frameskip", 0, "moshpit");
    
    CLASS_ATTR_LONG(c, "framerate", 0, t_moshpit, framerate);
    CLASS_ATTR_STYLE_LABEL(c, "framerate", 0, 0, "ms between renders");
    //CLASS_ATTR_SAVE(c, "flock", 0);
    CLASS_ATTR_CATEGORY(c, "framerate", 0, "moshpit");
    
    CLASS_ATTR_CHAR(c, "showforce", 0, t_moshpit, showforce);
    CLASS_ATTR_STYLE_LABEL(c, "showforce", 0, "onoff", "Show Force");
    //CLASS_ATTR_SAVE(c, "showforce", 0);
    CLASS_ATTR_CATEGORY(c, "showforce", 0, "moshpit");
    
    CLASS_ATTR_LONG(c, "boxsize", 0, t_moshpit, lx);
    CLASS_ATTR_STYLE_LABEL(c, "boxsize", 0, 0, "Box Size");
    //CLASS_ATTR_SAVE(c, "boxsize", 0);
    CLASS_ATTR_CATEGORY(c, "boxsize", 0, "moshpit");

	CLASS_STICKY_ATTR(c, "category", 0, "Color");
	CLASS_ATTR_RGBA(c, "bgcolor", 0, t_moshpit, u_background);
	CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c, "bgcolor", 0, "1. 1. 1. 1.");
	CLASS_ATTR_STYLE_LABEL(c,"bgcolor",0,"rgba","Background Color");

	CLASS_ATTR_RGBA(c, "bordercolor", 0, t_moshpit, u_outline);
	CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c, "bordercolor", 0, "0. 0. 0. 1.");
	CLASS_ATTR_STYLE_LABEL(c,"bordercolor",0,"rgba","Border Color");

	CLASS_ATTR_RGBA(c, "redcolor", 0, t_moshpit, u_red);
	CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c, "redcolor", 0, "1. 0. 0. 0.8");
	CLASS_ATTR_STYLE_LABEL(c,"redcolor",0,"rgba","Red Color");
    
    CLASS_ATTR_RGBA(c, "greycolor", 0, t_moshpit, u_grey);
    CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c, "greycolor", 0, "0.5 0.5 0.5 0.8");
    CLASS_ATTR_STYLE_LABEL(c,"greycolor",0,"rgba","Grey Color");

	// color uses the color declared in t_jbox.b_color
	CLASS_ATTR_DEFAULTNAME_SAVE_PAINT(c, "color", 0, "0. 0. 0. 1.");
	CLASS_ATTR_STYLE_LABEL(c,"color",0,"rgba","Check Color");

	CLASS_STICKY_ATTR_CLEAR(c, "category");

	CLASS_ATTR_DEFAULT(c,"patching_rect",0, "0. 0. 200. 200.");

	class_register(CLASS_BOX, c);
	s_moshpit_class = c;
}

void moshpit_assist(t_moshpit *x, void *b, long m, long a, char *s)
{
	if (m == 1)		//inlet
		sprintf(s, "(signal) Audio Input");
}

double normRand() {
    return (double)rand() / (double)RAND_MAX;
}

double mymod(a, b) { //TODO: MZ why?
    return a - b * floor(a/b) + b * (a < 0);
}

long moshpit_mod_rvec(int a, int b, int p, int *image) {
    *image = 1;
    if (b == 0) {
        if (a == 0){
            *image = 0;
        }
        return 0;
    }
    if (p != 0) {
        if (a > b) {
            return a - b - 1;
        }
        if (a < 0) {
            return a + b + 1;
        }
    } else {
        if (a>b) {
            return b;
        }
        if (a < 0) {
            return 0;
        }
    }
    *image = 0;
    return a;
}

double moshpit_calc_vorticity(t_moshpit *x) {
    double vor = 0.0;
    double cmx = 0.0;
    double cmy = 0.0;
    long count = 0;
    
    for (int i = 0; i < x->numMoshers; ++i) {
        if (x->type[i] > 0) {
            cmx += x->mpX[i];
            cmy += x->mpY[i];
            count++;
        }
    }
    cmx /= count;
    cmy /= count;
    
    for (int i = 0; i < x->numMoshers; ++i) {
        if (x->type[i] > 0) {
            double tx = x->mpX[i] - cmx;
            double ty = x->mpY[i] - cmy;
            vor += (x->vx[i] * ty) - (x->vy[i] * tx);
            //ivor[i] = tv;
        }
    }
    //ivoravg = vor/count;
    return -vor/count;
}

void nbl_bin(t_moshpit *x) {
    for (int i = 0; i < (x->size[0] * x->size[1]); ++i) {
        x->count[i] = 0;
    }
    for (int i = 0; i < x->numMoshers; ++i) {
        long indX = floor(x->mpX[i]/x->lx * x->size[0]);
        long indY = floor(x->mpY[i]/x->ly * x->size[1]);
        long tt = indX + indY * x->size[0];
        x->cells[NMAX*tt + x->count[tt]] = i;
        x->count[tt]++;
    }
}

void moshpit_update(t_moshpit *x) {
    x->colavg = 0.0;
    int image[2] = {0, 0};
    for(int i = 0; i <  x->numMoshers; ++i) {
        x->col[i] = 0.0;
        x->fx[i] = 0.0;
        x->fy[i] = 0.0;
        double wx = 0.0;
        double wy = 0.0;
        long neigh = 0;
        
        long indX = floor(x->mpX[i]/x->lx * x->size[0]);
        long indY = floor(x->mpY[i]/x->ly * x->size[1]);
        
        for (int ttx=-1; ttx <= 1; ++ttx){
            for (int tty=-1; tty <= 1; ++tty) {
                bool goodcell = 1;
                long tixx = moshpit_mod_rvec(indX + ttx, x->size[0]-1, x->pbc[0], &image[0]);
                long tixy = moshpit_mod_rvec(indY + tty, x->size[1]-1, x->pbc[1], &image[1]);
                if ((x->pbc[0] < image[0]) || (x->pbc[1] < image[1])) {
                    goodcell = 0;
                }
                
                if (goodcell) {
                    long cell = tixx + (tixy * x->size[0]);
                    for (int cc = 0; cc < x->count[cell]; ++cc){
                        long j = x->cells[NMAX*cell + cc];
                        double dx = x->mpX[j] - x->mpX[i];
                        if (image[0]) {
                            dx += x->lx * ttx;
                        }
                        double dy = x->mpY[j] - x->mpY[i];
                        if (image[1]) {
                            dy += x->ly * tty;
                        }
                        double l = sqrt(dx*dx + dy*dy);
                        if (l > 1e-6 && l < TWO_R) {
                            double r0 = x->r[i] + x->r[j];
                            double f = (1-l/r0);
                            double c0 = -(x->epsilon) * f*f * (l<r0);
                            x->fx[i] += c0*dx;
                            x->fy[i] += c0*dy;
                            double tcol = c0*c0*dx*dx + c0*c0*dy*dy; //fx[i]*fx[i] + fy[i]*fy[i]
                            x->col[i] += tcol;
                        }
                        if (x->type[i] > 0 && x->type[j] > 0 && l > 1e-6 && l < FR) {
                            wx += x->vx[j];
                            wy += x->vy[j];
                            neigh++;
                        }
                    }
                }
            }
        }
        double wlen = (wx*wx + wy*wy);
        if (x->type[i] > 0 && neigh > 0 && wlen > 1e-6) {
            x->fx[i] += x->flock * wx / wlen;
            x->fy[i] += x->flock * wy /wlen;
        }
        double vlen = x->vx[i]*x->vx[i] + x->vy[i]*x->vy[i];
        double vhap = 0.;
        if (x->type[i] > 0) {
            vhap = VHAPPY;
        } else {
            vhap = 0.;
        }
        if (vlen > 1e-6) {
            x->fx[i] += DAMP*(vhap - vlen)*x->vx[i]/vlen;
            x->fy[i] += DAMP*(vhap - vlen)*x->vy[i]/vlen;
        }
        if (x->type[i] > 0) {
            x->fx[i] += x->noise * (normRand()-0.5);
            x->fy[i] += x->noise * (normRand()-0.5);
        }
        //Some keys stuff here, which I ignored
        x->colavg += x->col[i];
    }
    for (int i = 0; i < x->numMoshers; ++i) {
        x->vx[i] += x->fx[i] * GDT;
        x->vy[i] += x->fy[i] * GDT;
        x->mpX[i] += x->vx[i] * GDT;
        x->mpY[i] += x->vy[i] * GDT;
        
        if (x->pbc[0] == 0) {
            if(x->mpX[i] >= x->lx) {
                x->mpX[i] = 2 * x->lx - x->mpX[i];
                x->vx[i] *= -1;
            }
            if (x->mpX[i] < 0) {
                x->mpX[i] = -(x->mpX[i]);
                x->vx[i] *= -1;
            }
            
        } else {
            if (x->mpX[i] >= x->lx || x->mpX[i] < 0) {
                x->mpX[i] = mymod(x->mpX[i], x->lx);
            }
        }
        if (x->pbc[1] == 0) {
            if(x->mpY[i] >= x->ly) {
                x->mpY[i] = 2 * x->ly - x->mpY[i];
                x->vy[i] *= -1;
            }
            if (x->mpY[i] < 0) {
                x->mpY[i] = -(x->mpY[i]);
                x->vy[i] *= -1;
            }
            
        } else {
            if (x->mpY[i] >= x->ly || x->mpY[i] < 0) {
                x->mpY[i] = mymod(x->mpY[i], x->ly);
            }
        }
    /* TODO: Do I need this?
        if (dovorticity == true) {
            graph_vel(sqrt(x->vx[i]*x->vx[i] + x->vy[i]* x->vy[i]));
        }*/

        
    }
    x->colavg /= x->numMoshers;
}

void moshpit_draw_all(t_moshpit *x, t_rect rect, t_jgraphics *g) {
    double sx = rect.width/x->lx;
    double sy = rect.height/x->ly;
    double ss = sqrt(sx*sy);
    double cr = 0.;
    for (int i = 0; i < x->numMoshers; ++i) {
        cr = fabs(x->col[i]/25);
        if (cr < 0.) {
            cr = 0.0;
        } else if (cr > 1.) {
            cr = 1.0;
        }
        if (x->type[i] == 0) {
            if (x->showforce == 1) {
                jgraphics_set_source_rgba(g, cr, cr, cr, 0.8);
            } else {
                jgraphics_set_source_jrgba(g, &x->u_grey);
            }
        } else if (x->type[i] == 2) {
            if (x->showforce == 1) {
                jgraphics_set_source_rgba(g, 1., 1., 0., cr);
            } else {
                jgraphics_set_source_rgba(g, 1., 1., 0., 0.8);
            }
        } else {
            if (x->showforce == 1) {
                jgraphics_set_source_rgba(g, 1., 0., 0., 0.8);
            } else {
                jgraphics_set_source_jrgba(g, &x->u_red);
            }
        }
        if (x->type[i] == 2) {
            t_atom outList[3];
            atom_setlong(outList, sx*x->mpX[i]);
            atom_setlong(outList+1, sy*x->mpY[i]);
            atom_setlong(outList+2, cr*100);
            outlet_list(x->m_out,0L, 3, outList);
        }
        jgraphics_set_line_width(g, 1.);
        jgraphics_arc(g, sx*x->mpX[i], sy*x->mpY[i], ss*x->r[i], 0, 2*M_PI);
        jgraphics_fill(g);
        jgraphics_stroke(g);
    }
}

void moshpit_paint(t_moshpit *x, t_object *patcherview)
{
	t_rect rect;
	t_jgraphics *g = (t_jgraphics *) patcherview_get_jgraphics(patcherview);		// obtain graphics context
	jbox_get_rect_for_view((t_object *)x, patcherview, &rect);

	// paint outline
	jgraphics_set_source_jrgba(g, &x->u_outline);
	jgraphics_set_line_width(g, 1.);
	jgraphics_rectangle(g, 0., 0., rect.width, rect.height);
	jgraphics_stroke(g);
    
    nbl_bin(x);
    for (int i = 0; i < x->frameskip; ++i) {
        moshpit_update(x);
    }
    moshpit_draw_all(x, rect, g);
}

void moshpit_task(t_moshpit *x){
    jbox_redraw((t_jbox *)x);
    clock_fdelay(x->m_clock, x->framerate);
}

// mouse interaction

void moshpit_mousedown(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	x->u_mousedowninside = true;	// wouldn't get a click unless it was inside the box
	jbox_redraw((t_jbox *)x);
}

void moshpit_mousedrag(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	//nothing
}

void moshpit_mouseup(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	//nothing
}

void moshpit_mouseenter(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	//nothing
}

void moshpit_mouseleave(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	//nothing
}

void moshpit_mousemove(t_moshpit *x, t_object *patcherview, t_pt pt, long modifiers)
{
	// nothing to do here, but could track mouse position inside object
}

void moshpit_int(t_moshpit *x, long n)
{
    if (n > 0) {
        clock_fdelay(x->m_clock, 1.);
    } else {
        clock_unset(x->m_clock);
    }
}

void moshpit_getdrawparams(t_moshpit *x, t_object *patcherview, t_jboxdrawparams *params)
{
	params->d_bordercolor.alpha = 0;
	params->d_boxfillcolor.alpha = 0;
}

void moshpit_free(t_moshpit *x)
{
    object_free(x->m_clock);
	jbox_free((t_jbox *)x);
}

void init_empty(t_moshpit *x) {
    for (int i = 0; i < x->numMoshers; ++i) {
        x->r[i] = 0.;
        x->mpX[i] = 0.;
        x->mpY[i] = 0.;
        x->type[i] = 0;
        x->vx[i] = 0.;
        x->vy[i] = 0.;
        x->fx[i] = 0.;
        x->fy[i] = 0.;
        x->col[i] = 0.;
    }
}

long calc_sidelength(t_moshpit *x) {
    return floor(1.03 * sqrt(M_PI * RADIUS * RADIUS * x->numMoshers));
}

void init_sidelength(t_moshpit *x, long L) {
    x->lx = L;
    x->ly = x->lx;
    
    //neighborlist
    x->size[0] = floor(x->lx/FR);
    x->size[1] = floor (x->ly/FR);
    for (int i = 0; i < (x->size[0] * x->size[1] * NMAX); ++i) {
        x->cells[i] = 0;
    }
    for (int i = 0; i < (x->size[0] * x->size[1]); ++i) {
        x->count[i] = 0;
    }
}

void init_circle (t_moshpit *x) {
    bool uniq = true;
    for (int i = 0; i < x->numMoshers; ++i) {
        double tx = x->lx * normRand();
        double ty = x->ly * normRand();
        
        x->type[i] = 0;
        x->r[i] = RADIUS;
        x->mpX[i] = tx;
        x->mpY[i] = ty;
        double dd = sqrt((tx - x->lx/2)*(tx - x->lx/2) + (ty - x->ly/2)*(ty - x->ly/2));
        double rad = sqrt(FRAC * x->lx * x->ly / M_PI);
        bool doCircle = true;
        if (doCircle) {
            if (dd < rad) {
                x->type[i] = (uniq) ? 2 : 1;
                uniq = false;
            }
        } else {
            if ( normRand() < FRAC ) {
                x->type[i] = 1;
            }
        }
        x->vx[i] = VHAPPY * (normRand() - 0.5);
        x->vy[i] = VHAPPY * (normRand() - 0.5);
    }
}

void moshpit_bang(t_moshpit *x)
{
    init_empty(x);
    init_sidelength(x, calc_sidelength(x));
    init_circle(x);
    jbox_redraw((t_jbox *)x);
}

void *moshpit_new(t_symbol *s, long argc, t_atom *argv)
{
	t_moshpit *x = NULL;
	t_dictionary *d = NULL;
	long boxflags;

	if (!(d = object_dictionaryarg(argc,argv)))
		return NULL;

	x = (t_moshpit *)object_alloc(s_moshpit_class);
	boxflags = 0
			   | JBOX_DRAWFIRSTIN
			   | JBOX_NODRAWBOX
			   | JBOX_DRAWINLAST
			   | JBOX_TRANSPARENT
			   //		| JBOX_NOGROW
			   | JBOX_GROWY
			   //		| JBOX_GROWBOTH
			   //		| JBOX_HILITE
			   //		| JBOX_BACKGROUND
			   | JBOX_DRAWBACKGROUND
			   //		| JBOX_NOFLOATINSPECTOR
			   //		| JBOX_TEXTFIELD
			   //		| JBOX_MOUSEDRAGDELTA
			   //		| JBOX_TEXTFIELD
			   ;

	jbox_new((t_jbox *)x, boxflags, argc, argv);
    x->u_box.b_firstin = (void *)x;
	x->u_mousedowninside = x->u_mouseover = x->u_state = 0;
    x->numMoshers = 300;
    x->size[0] = 0;
    x->size[1] = 0;
    x->pbc[0] = 1;
    x->pbc[1] = 1;
    x->epsilon = 100;
    x->flock = 1.0;
    x->noise = 3.0;
    x->showforce = 0;
    x->frameskip = 2;
    x->framerate = 30;
    init_empty(x);
    init_sidelength(x, calc_sidelength(x));
    init_circle(x);
    x->m_clock = clock_new((t_object *)x, (method)moshpit_task);
	x->m_out = listout((t_object *)x);
	attr_dictionary_process(x,d);
	jbox_ready((t_jbox *)x);
	return x;
}
