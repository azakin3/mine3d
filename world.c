
#include "config.h"

#include "world.h"

#include "utils.h"
#include "noise.h"
#include "block.h"
#include "utils.h"

#include "SDL.h"
#include "glew.h"
#include <math.h>
#include <stdlib.h>
#include <pthread.h>

private Segment* newSegment();

public Block segmentGet(Segment* this, int x, int y, int z)
{
    if(x<0 || y<0 || z<0 || x>=SEGMENT_SIZE || y>=SEGMENT_SIZE || z>=SEGMENT_SIZE)
        return (Block){};
    else
        return this->data[z][y][x];
}

public Block worldGet(World* this, Vec4i pos)
{

    const Vec4i segment_bits=(Vec4i){SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS};
    const Vec4i segment_mask=(Vec4i){SEGMENT_MASK,SEGMENT_MASK,SEGMENT_MASK,SEGMENT_MASK};

    Vec4i global=(pos>>segment_bits)-this->scroll;

    for(int i=0;i<3;i++)
        if(global[i]<0 || global[i]>=VIEW_RANGE)
            return (Block){};

    Segment* segment=this->segment[global[2]][global[1]][global[0]];

    if(segment==NULL)
        return (Block){.skyLight=1.0};

    Vec4i local=pos&segment_mask;

    return segment->data[local[2]][local[1]][local[0]];

}

private void worldTouch(World* this, Vec4i pos)
{

    const Vec4i segment_bits=(Vec4i){SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS};

    Vec4i global=(pos>>segment_bits)-this->scroll;

    for(int i=0;i<3;i++)
        if(global[i]<0 || global[i]>=VIEW_RANGE)
            return;

    Segment* segment=this->segment[global[2]][global[1]][global[0]];

    if(segment!=NULL)
    {
        segment->rendered=false;
    }

}

public void worldSet(World* this, Vec4i pos, Block block)
{

    const Vec4i segment_bits=(Vec4i){SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS,SEGMENT_BITS};
    const Vec4i segment_mask=(Vec4i){SEGMENT_MASK,SEGMENT_MASK,SEGMENT_MASK,SEGMENT_MASK};

    Vec4i global=(pos>>segment_bits)-this->scroll;

    for(int i=0;i<3;i++)
        if(global[i]<0 || global[i]>=VIEW_RANGE)
            return;

    Segment* segment=this->segment[global[2]][global[1]][global[0]];

    if(segment==NULL)
    {
        segment=newSegment();
        this->segment[global[2]][global[1]][global[0]]=segment;
    }

    worldTouch(this,pos+(Vec4i){0,0, 1});
    worldTouch(this,pos+(Vec4i){0,0,-1});
    worldTouch(this,pos+(Vec4i){ 1,0,0});
    worldTouch(this,pos+(Vec4i){-1,0,0});
    worldTouch(this,pos+(Vec4i){0, 1,0});
    worldTouch(this,pos+(Vec4i){0,-1,0});

    segment->rendered=false;

    if(block.id!=0)
        segment->empty=false;

    Vec4i local=pos&segment_mask;

    segment->data[local[2]][local[1]][local[0]]=block;

}

public Vec4i worldRay(World* world, Vec4f pos0, Vec4f normal, int max_length)
{

    Vec4i pos1=(Vec4i){floor(pos0[0]),floor(pos0[1]),floor(pos0[2])};
    Vec4i step=(Vec4i){sign(normal[0]),sign(normal[1]),sign(normal[2])};
    Vec4f delta=(Vec4f){fabs(1/normal[0]),fabs(1/normal[1]),fabs(1/normal[2])};
    Vec4f max;

    for(int d=0;d<3;d++)
    {

        if(normal[d]==0)
            max[d]=1.0/0.0;
        else if(normal[d]<0)
            max[d]=-fract(pos0[d])/normal[d];
        else
            max[d]=(1-fract(pos0[d]))/normal[d];

    }

    while(true)
    {

        int i;

        if(max[0]<=max[1] && max[0]<=max[2])
            i=0;
        else if(max[1]<=max[0] && max[1]<=max[2])
            i=1;
        else if(max[2]<=max[0] && max[2]<=max[1])
            i=2;
        else
            panic("unreachable");

        pos1[i]+=step[i];
        max[i]+=delta[i];

        if(abs(pos1[i]-pos0[i])>max_length)
            break;

        if(worldGet(world,pos1).id!=0)
        {
            int face=(i<<1)|(step[i]<0);
            pos1[3]=face;
            return pos1;
        }
    }

    return (Vec4i){0,0,0,-1};

}

private void renderSegment(World* world, Segment* this, Vec4i pos)
{

    assert(this!=NULL);

    this->rendered=true;

    if(this->empty)
        return;

    static Vertex data[SEGMENT_SIZE*SEGMENT_SIZE*SEGMENT_SIZE*6*4];

    VertexBuffer buffer=
    {
        .maxSize=sizeof(data)/sizeof(*data),
        .size=0,
        .data=data,
    };

    for(int z=0;z<SEGMENT_SIZE;z++)
    for(int y=0;y<SEGMENT_SIZE;y++)
    for(int x=0;x<SEGMENT_SIZE;x++)
    {
        blockDraw(world,pos*SEGMENT_SIZEV+(Vec4i){x,y,z},&buffer);
    }

    if(buffer.size==0)
        return;

    this->n=buffer.size;

    if(this->vbo==0)
        glGenBuffers(1,&this->vbo);

    glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(*data)*this->n, data, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

}

private void drawSegment(World* world, Segment* this, Vec4i pos)
{

    if(this!=NULL && this->vbo!=0)
    {

        glBindBuffer(GL_ARRAY_BUFFER, this->vbo);
        glVertexPointer(3, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex,pos));
        glColorPointer(4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex,color));
        glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex), (void*)offsetof(Vertex,texCoord));

        glDrawArrays(GL_QUADS, 0, this->n);

    }

}

static int allocated_segments=0;

private Segment* newSegment()
{
    allocated_segments++;
    return calloc(1,sizeof(Segment));
}

private void freeSegment(Segment* this)
{

    if(this==NULL)
        return;

    allocated_segments--;

    if(this->vbo!=0)
        glDeleteBuffers(1,&this->vbo);

    free(this);

}

private void worldSpiral(World* this, void (f)(World*,int x,int y,int z))
{

    int c=VIEW_RANGE/2;

    for(int r=0;r<c;r++)
    for(int y=-r;y<r;y++)
    for(int x=-r;x<r;x++)
    {
        if(y==-r || x==-r || y==r-1 || x==r-1)
        {
            for(int z=-r;z<r;z++)
                f(this,c+x,c+y,c+z);
        }
        else
        {
            f(this,c+x,c+y,c+r-1);
            f(this,c+x,c+y,c-r);
        }
    }

}

public bool worldEvent(World* this, const SDL_Event* event)
{

    switch(event->type)
    {
        case SDL_KEYDOWN:
            switch(event->key.keysym.sym)
            {
                case SDLK_f:
                    this->player.flying=!this->player.flying;
                    return true;
                case SDLK_SPACE:
                    this->player.v[2]=0.31;
                    return true;
                default:
                    return false;
            }
        case SDL_MOUSEMOTION:
            this->player.rot[0]-=event->motion.xrel/100.0;
            this->player.rot[1]-=event->motion.yrel/100.0;
            this->player.rot[1]=clampf(this->player.rot[1],0,M_PI);
            return false;
        default:
            return true;
    }
}

private void playerTick(World* world)
{
    //int t=SDL_GetTicks();
    Uint8 *keys = SDL_GetKeyState(NULL);

    double vx=0,vy=0;
    double v=0.15;

    if(keys[SDLK_w] || keys[SDLK_UP])
    {
        vy+=1;
    }
    if(keys[SDLK_s] || keys[SDLK_DOWN])
    {
        vy-=1;
    }
    if(keys[SDLK_a] || keys[SDLK_LEFT])
    {
        vx-=1;
    }
    if(keys[SDLK_d] || keys[SDLK_RIGHT])
    {
        vx+=1;
    }
    if(keys[SDLK_LSHIFT] || keys[SDLK_RSHIFT])
    {
        //v=0.01*world->player.pos[0];
        v=0.1;
    }

    if(!world->player.flying)
    {
        world->player.v[0]=cos(world->player.rot[0])*vx*v-sin(world->player.rot[0])*vy*v;
        world->player.v[1]=-sin(world->player.rot[0])*vx*v-cos(world->player.rot[0])*vy*v;

        actorTick(world,&world->player);
    }
    else
    {

        world->player.v[0]=-sin(world->player.rot[0])*sin(world->player.rot[1])*vy*v+cos(world->player.rot[0])*vx*v;
        world->player.v[1]=-cos(world->player.rot[0])*sin(world->player.rot[1])*vy*v-sin(world->player.rot[0])*vx*v;
        world->player.v[2]=-cos(world->player.rot[1])*vy*v;

        actorTick(world,&world->player);

    }

}

public void worldTick(World* this)
{

    this->ticks=this->lastSyncTicks+(SDL_GetTicks()-this->lastSyncTime)*20/1000;

    playerTick(this);


    Vec4i scroll=
    {
        this->player.pos[0]/SEGMENT_SIZE-VIEW_RANGE/2,
        this->player.pos[1]/SEGMENT_SIZE-VIEW_RANGE/2,
        this->player.pos[2]/SEGMENT_SIZE-VIEW_RANGE/2,
    };

    if(scroll[0]!=this->scroll[0] || scroll[1]!=this->scroll[1] || scroll[2]!=this->scroll[2])
    {

        Segment* segment[VIEW_RANGE][VIEW_RANGE][VIEW_RANGE]={};
        Vec4i delta=scroll-this->scroll;

        for(int z=0;z<VIEW_RANGE;z++)
        for(int y=0;y<VIEW_RANGE;y++)
        for(int x=0;x<VIEW_RANGE;x++)
        {

            Segment* s=this->segment[z][y][x];

            int ix=x-delta[0];
            int iy=y-delta[1];
            int iz=z-delta[2];

            if(ix<0 || iy<0 || iz<0 || ix>=VIEW_RANGE || iy>=VIEW_RANGE || iz>=VIEW_RANGE)
            {
                freeSegment(s);
            }
            else
            {
                segment[iz][iy][ix]=s;
            }

        }

        memcpy(this->segment,segment,sizeof(segment));
        this->scroll=scroll;

    }

    //if(this->socket!=NULL)
    //  sendPlayerPositionAndLook(this, this->socket);

    //printf("ticks: %i ",SDL_GetTicks()-t);
    //printf("segments: %i ",allocated_segments);
    //printf("player: %f %f %f\n",this->player.pos[0],this->player.pos[1],this->player.pos[2]);

}

private void worldDrawSegment(World *this,int x, int y, int z)
{

    Vec4i pos=(Vec4i){x,y,z}+this->scroll;

/*
    if(this->segment[z][y][x]==NULL && SDL_GetTicks()-this->time<10)
    {
        this->segment[z][y][x]=newSegment();
        generateSegment(this, this->segment[z][y][x],(Vec4i){x,y,z}+this->scroll);
    }
*/

    if( this->segment[z][y][x]!=NULL && this->segment[z][y][x]->rendered==false && SDL_GetTicks()-this->drawStart<10)
    {
        renderSegment(this, this->segment[z][y][x], pos);
    }

    drawSegment(this, this->segment[z][y][x], pos);

}

private void drawSelection(World* world, Vec4i pos)
{
    static const Vec4i face[6][4]={
        {{0,0,0},{0,1,0},{0,1,1},{0,0,1}},
        {{1,1,0},{1,0,0},{1,0,1},{1,1,1}},

        {{1,0,0},{0,0,0},{0,0,1},{1,0,1}},
        {{0,1,0},{1,1,0},{1,1,1},{0,1,1}},

        {{0,0,0},{1,0,0},{1,1,0},{0,1,0}},
        {{0,0,1},{0,1,1},{1,1,1},{1,0,1}},
    };

    glDisable(GL_DEPTH_TEST);
    glBegin(GL_LINE_STRIP);
    for(int i=0;i<4;i++)
        glVertexi(pos+face[pos[3]][i]);
    glVertexi(pos+face[pos[3]][0]);
    glEnd();

}

public void worldDraw(World *world)
{

    world->drawStart=SDL_GetTicks();

    char buf[1024];
    sprintf(buf,"x:%f y:%f z:%f (%f %f)",world->player.pos[0],world->player.pos[1],world->player.pos[2],world->player.rot[0],world->player.rot[1]);
    //ftglRenderFont(world->font, buf, FTGL_RENDER_ALL);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();

    //glTranslated(0,0,-3);


    glRotatef(world->player.rot[1]*180/M_PI,1,0,0);
    glRotatef(world->player.rot[0]*180/M_PI,0,0,1);

    glTranslatev(-world->player.pos-world->player.headOffset);

    glColor3f(1.0,1.0,1.0);
    glBegin(GL_LINES);
    int n=16;
    for(int i=-n;i<n;i++)
    {
        glVertex3f(-16*n,i*16,0);
        glVertex3f(16*n,i*16,0);
        glVertex3f(i*16,-16*n,0);
        glVertex3f(i*16,16*n,0);
    }
    glEnd();

    //actorDrawBBox(&world->player);

    worldTick(world);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glAlphaFunc(GL_GREATER,0.1);

    //float daytime=(world->ticks%24000ull)/24000.0;

    glMatrixMode(GL_MODELVIEW);

    glBindTexture(GL_TEXTURE_2D, world->terrain);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    worldSpiral(world,worldDrawSegment);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);


    Vec4i p=worldRay(world, world->player.pos+world->player.headOffset,  rotationNormal(world->player.rot),3);

    if(p[3]!=-1)
        drawSelection(world,p);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

}


public void worldInit(World *this)
{

    *this=(World){};

    this->player=(Player){.size={0.3,0.3,0.9},.headOffset={0.0,0.0,0.89}};

    noiseInit(&this->noise,666);

}

public void worldDisplayInit(World *this)
{
    this->terrain=loadTexture("data/terrain.png");
    assert(this->terrain!=0);

    this->font=ftglCreateBitmapFont("/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf");
    assert(this->font!=NULL);
    ftglSetFontFaceSize(this->font, 18, 0);
}

public void worldDestroy(World* this)
{

    glDeleteTextures(1,&this->terrain);

    for(int z=0;z<VIEW_RANGE;z++)
    for(int y=0;y<VIEW_RANGE;y++)
    for(int x=0;x<VIEW_RANGE;x++)
    {
        freeSegment(this->segment[z][y][x]);
    }

}

