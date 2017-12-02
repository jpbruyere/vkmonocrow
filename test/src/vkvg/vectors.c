#include "vectors.h"

vec2d _v2LineNorm(vec2 a, vec2 b)
{
    vec2d d = {b.x - a.x, b.y - a.y};
    double md = sqrt (d.x*d.x + d.y*d.y);
    d.x/=md;
    d.y/=md;
    return d;
}
double _v2Lengthd(vec2d v){
    return sqrt (v.x*v.x + v.y*v.y);
}
float _v2Length(vec2 v){
    return sqrt (v.x*v.x + v.y*v.y);
}
vec2d _v2Normd(vec2d a)
{
    double m = sqrt (a.x*a.x + a.y*a.y);
    vec2d d = {a.x/m, a.y/m};
    return d;
}
vec2d _v2Multd(vec2d a, double m){
    vec2d r = {a.x*m,a.y*m};
    return r;
}

vec2d _v2LineNormd(vec2d a, vec2d b)
{
    vec2d d = {b.x - a.x, b.y - a.y};
    double md = sqrt (d.x*d.x + d.y*d.y);
    d.x/=md;
    d.y/=md;
    return d;
}
vec2d _v2dPerpd (vec2d a){
    vec2d vp = {a.y, -a.x};
    return vp;
}
vec2d _v2perpNorm(vec2 a, vec2 b)
{
    vec2d d = {b.x - a.x, b.y - a.y};
    double md = sqrt (d.x*d.x + d.y*d.y);
    vec2d vpn = {d.y/md, -d.x/md};
    return vpn;
}
vec2 _vec2dToVec2(vec2d vd){
    vec2 v = {vd.x,vd.y};
    return v;
}
vec2 _v2perpNorm2(vec2 a, vec2 b)
{
    vec2 c = {a.x - b.x, a.y - b.y};
    vec2 d = {-c.y/c.x, 1};
    return d;
}
vec2 vec2_add (vec2 a, vec2 b){
    vec2 r = {a.x + b.x, a.y + b.y};
    return r;
}
vec2d _v2addd (vec2d a, vec2d b){
    vec2d r = {a.x + b.x, a.y + b.y};
    return r;
}
vec2 _v2sub (vec2 a, vec2 b){
    vec2 r = {a.x - b.x, a.y - b.y};
    return r;
}
vec2d _v2subd (vec2d a, vec2d b){
    vec2d r = {a.x - b.x, a.y - b.y};
    return r;
}
bool _v2equ (vec2 a, vec2 b){
    if ((a.x == b.x) && (a.y == b.y))
        return true;
    return false;
}
