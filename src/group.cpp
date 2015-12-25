//-----------------------------------------------------------------------------
// Implementation of the Group class, which represents a set of entities and
// constraints that are solved together, in some cases followed by another
// operation, like to extrude surfaces from the entities or to step and
// repeat them parametrically.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

const hParam   Param::NO_PARAM = { 0 };
#define NO_PARAM (Param::NO_PARAM)

const hGroup Group::HGROUP_REFERENCES = { 1 };

#define gs (SS.GW.gs)

//-----------------------------------------------------------------------------
// The group structure includes pointers to other dynamically-allocated
// memory. This clears and frees them all.
//-----------------------------------------------------------------------------
void Group::Clear(void) {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();
    thisMesh.Clear();
    runningMesh.Clear();
    thisShell.Clear();
    runningShell.Clear();
    displayMesh.Clear();
    displayEdges.Clear();
    impMesh.Clear();
    impShell.Clear();
    impEntity.Clear();
    // remap is the only one that doesn't get recreated when we regen
    remap.Clear();
}

void Group::AddParam(IdList<Param,hParam> *param, hParam hp, double v) {
    Param pa;
    memset(&pa, 0, sizeof(pa));
    pa.h = hp;
    pa.val = v;

    param->Add(&pa);
}

bool Group::IsVisible(void) {
    if(!visible) return false;
    if(SS.GroupsInOrder(SS.GW.activeGroup, h)) return false;
    return true;
}

void Group::MenuGroup(int id) {
    Group g;
    ZERO(&g);
    g.visible = true;
    g.color = RGBi(100, 100, 100);
    g.scale = 1;

    if(id >= RECENT_IMPORT && id < (RECENT_IMPORT + MAX_RECENT)) {
        strcpy(g.impFile, RecentFile[id-RECENT_IMPORT]);
        id = GraphicsWindow::MNU_GROUP_IMPORT;
    }

    SS.GW.GroupSelection();

    switch(id) {
        case GraphicsWindow::MNU_GROUP_3D:
            g.type = DRAWING_3D;
            g.name.strcpy("sketch-in-3d");
            break;

        case GraphicsWindow::MNU_GROUP_WRKPL:
            g.type = DRAWING_WORKPLANE;
            g.name.strcpy("sketch-in-plane");
            if(gs.points == 1 && gs.n == 1) {
                g.subtype = WORKPLANE_BY_POINT_ORTHO;

                Vector u = SS.GW.projRight, v = SS.GW.projUp;
                u = u.ClosestOrtho();
                v = v.Minus(u.ScaledBy(v.Dot(u)));
                v = v.ClosestOrtho();

                g.predef.q = Quaternion::From(u, v);
                g.predef.origin = gs.point[0];
            } else if(gs.points == 1 && gs.lineSegments == 2 && gs.n == 3) {
                g.subtype = WORKPLANE_BY_LINE_SEGMENTS;

                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.entity[0];
                g.predef.entityC = gs.entity[1];

                Vector ut = SK.GetEntity(g.predef.entityB)->VectorGetNum();
                Vector vt = SK.GetEntity(g.predef.entityC)->VectorGetNum();
                ut = ut.WithMagnitude(1);
                vt = vt.WithMagnitude(1);

                if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
                    swap(ut, vt);
                    g.predef.swapUV = true;
                }
                if(SS.GW.projRight.Dot(ut) < 0) g.predef.negateU = true;
                if(SS.GW.projUp.   Dot(vt) < 0) g.predef.negateV = true;
            } else if(gs.faces == 1 && gs.n == 1) {
                g.subtype = WORKPLANE_BY_FACE;

                // Remember the face used to create workplane.
                g.predef.entityB = gs.face[0];

                // Calculate workplane basis.
                Entity *face = SK.GetEntity(g.predef.entityB);
                Quaternion q = face->FaceGetQuatNum();
                Vector ut = q.RotationU();
                Vector vt = q.RotationV();

                // Choose workplane orientation based on the current point of view.
                if(fabs(SS.GW.projUp.Dot(vt)) < fabs(SS.GW.projUp.Dot(ut))) {
                    swap(ut, vt);
                    g.predef.swapUV = true;
                }
                if(SS.GW.projRight.Dot(ut) < 0) g.predef.negateU = true;
                if(SS.GW.projUp.   Dot(vt) < 0) g.predef.negateV = true;
            } else {
                Error("Bad selection for new sketch in workplane. This "
                      "group can be created with:\n\n"
                      "    * a point (orthogonal to coordinate axes, "
                             "through the point)\n"
                      "    * a point and two line segments (parallel to the "
                             "lines, through the point)\n"
                      "    * a plane face\n");
                return;
            }
            break;

        case GraphicsWindow::MNU_GROUP_EXTRUDE:
            if(!SS.GW.LockedInWorkplane()) {
                Error("Select a workplane (Sketch -> In Workplane) before "
                      "extruding. The sketch will be extruded normal to the "
                      "workplane.");
                return;
            }
            g.type = EXTRUDE;
            g.opA = SS.GW.activeGroup;
            g.predef.entityB = SS.GW.ActiveWorkplane();
            g.subtype = ONE_SIDED;
            g.name.strcpy("extrude");
            break;

        case GraphicsWindow::MNU_GROUP_LATHE:
            if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                g.predef.origin = SK.GetEntity(gs.entity[0])->point[0];
                g.predef.entityB = gs.entity[0];
                // since a line segment is a vector
            } else {
                Error("Bad selection for new lathe group. This group can "
                      "be created with:\n\n"
                      "    * a point and a line segment or normal "
                               "(revolved about an axis parallel to line / "
                               "normal, through point)\n"
                      "    * a line segment (revolved about line segment)\n");
                return;
            }
            g.type = LATHE;
            g.opA = SS.GW.activeGroup;
            g.name.strcpy("lathe");
            break;

        case GraphicsWindow::MNU_GROUP_ROT: {
            if(gs.points == 1 && gs.n == 1 && SS.GW.LockedInWorkplane()) {
                g.predef.origin = gs.point[0];
                Entity *w = SK.GetEntity(SS.GW.ActiveWorkplane());
                g.predef.entityB = w->Normal()->h;
                g.activeWorkplane = w->h;
            } else if(gs.points == 1 && gs.vectors == 1 && gs.n == 2) {
                g.predef.origin = gs.point[0];
                g.predef.entityB = gs.vector[0];
            } else {
                Error("Bad selection for new rotation. This group can "
                      "be created with:\n\n"
                      "    * a point, while locked in workplane (rotate "
                            "in plane, about that point)\n"
                      "    * a point and a line or a normal (rotate about "
                            "an axis through the point, and parallel to "
                            "line / normal)\n");
                return;
            }
            g.type = ROTATE;
            g.opA = SS.GW.activeGroup;
            g.valA = 3;
            g.subtype = ONE_SIDED;
            g.name.strcpy("rotate");
            break;
        }

        case GraphicsWindow::MNU_GROUP_TRANS:
            g.type = TRANSLATE;
            g.opA = SS.GW.activeGroup;
            g.valA = 3;
            g.subtype = ONE_SIDED;
            g.predef.entityB = SS.GW.ActiveWorkplane();
            g.activeWorkplane = SS.GW.ActiveWorkplane();
            g.name.strcpy("translate");
            break;

        case GraphicsWindow::MNU_GROUP_IMPORT: {
            g.type = IMPORTED;
            g.opA = SS.GW.activeGroup;
            if(strlen(g.impFile) == 0) {
                if(!GetOpenFile(g.impFile, SLVS_EXT, SLVS_PATTERN)) return;
            }

            // Assign the default name of the group based on the name of
            // the imported file.
            char groupName[MAX_PATH];
            strcpy(groupName, g.impFile);
            char *dot = strrchr(groupName, '.');
            if(dot) *dot = '\0';

            char *s, *start = groupName;
            for(s = groupName; *s; s++) {
                if(*s == '/' || *s == '\\') {
                    start = s + 1;
                } else if(isalnum(*s)) {
                    // do nothing, valid character
                } else {
                    // convert invalid characters (like spaces) to dashes
                    *s = '-';
                }
            }
            if(strlen(start) > 0) {
                g.name.strcpy(start);
            } else {
                g.name.strcpy("import");
            }

            g.meshCombine = COMBINE_AS_ASSEMBLE;
            break;
        }

        default: oops();
    }
    SS.GW.ClearSelection();
    SS.UndoRemember();

    SK.group.AddAndAssignId(&g);
    Group *gg = SK.GetGroup(g.h);

    if(gg->type == IMPORTED) {
        SS.ReloadAllImported();
    }
    gg->clean = false;
    SS.GW.activeGroup = gg->h;
    SS.GenerateAll();
    if(gg->type == DRAWING_WORKPLANE) {
        // Can't set the active workplane for this one until after we've
        // regenerated, because the workplane doesn't exist until then.
        gg->activeWorkplane = gg->h.entity(0);
    }
    gg->Activate();
    SS.GW.AnimateOntoWorkplane();
    TextWindow::ScreenSelectGroup(0, gg->h.v);
    SS.ScheduleShowTW();
}

void Group::TransformImportedBy(Vector t, Quaternion q) {
    if(type != IMPORTED) oops();

    hParam tx, ty, tz, qw, qx, qy, qz;
    tx = h.param(0);
    ty = h.param(1);
    tz = h.param(2);
    qw = h.param(3);
    qx = h.param(4);
    qy = h.param(5);
    qz = h.param(6);

    Quaternion qg = Quaternion::From(qw, qx, qy, qz);
    qg = q.Times(qg);

    Vector tg = Vector::From(tx, ty, tz);
    tg = tg.Plus(t);

    SK.GetParam(tx)->val = tg.x;
    SK.GetParam(ty)->val = tg.y;
    SK.GetParam(tz)->val = tg.z;

    SK.GetParam(qw)->val = qg.w;
    SK.GetParam(qx)->val = qg.vx;
    SK.GetParam(qy)->val = qg.vy;
    SK.GetParam(qz)->val = qg.vz;
}

char *Group::DescriptionString(void) {
    static char ret[100];
    if(name.str[0]) {
        sprintf(ret, "g%03x-%s", h.v, name.str);
    } else {
        sprintf(ret, "g%03x-(unnamed)", h.v);
    }
    return ret;
}

void Group::Activate(void) {
    if(type == EXTRUDE || type == IMPORTED || type == LATHE || type == TRANSLATE || type == ROTATE) {
        SS.GW.showFaces = true;
    } else {
        SS.GW.showFaces = false;
    }
    SS.MarkGroupDirty(h); // for good measure; shouldn't be needed
    SS.ScheduleGenerateAll();
    SS.ScheduleShowTW();
}

void Group::Generate(IdList<Entity,hEntity> *entity,
                     IdList<Param,hParam> *param)
{
    Vector gn = (SS.GW.projRight).Cross(SS.GW.projUp);
    Vector gp = SS.GW.projRight.Plus(SS.GW.projUp);
    Vector gc = (SS.GW.offset).ScaledBy(-1);
    gn = gn.WithMagnitude(200/SS.GW.scale);
    gp = gp.WithMagnitude(200/SS.GW.scale);
    int a, i;
    switch(type) {
        case DRAWING_3D:
            break;

        case DRAWING_WORKPLANE: {
            Vector origin;
            Quaternion q;
            if(subtype == WORKPLANE_BY_LINE_SEGMENTS) {
                Vector u = SK.GetEntity(predef.entityB)->VectorGetNum();
                Vector v = SK.GetEntity(predef.entityC)->VectorGetNum();
                u = u.WithMagnitude(1);
                Vector n = u.Cross(v);
                v = (n.Cross(u)).WithMagnitude(1);

                if(predef.swapUV) swap(u, v);
                if(predef.negateU) u = u.ScaledBy(-1);
                if(predef.negateV) v = v.ScaledBy(-1);
                q = Quaternion::From(u, v);
                origin = SK.GetEntity(predef.origin)->PointGetNum();
            } else if(subtype == WORKPLANE_BY_POINT_ORTHO) {
                // Already given, numerically.
                q = predef.q;
                origin = SK.GetEntity(predef.origin)->PointGetNum();
            } else if(subtype == WORKPLANE_BY_FACE) {
                Entity *face = SK.GetEntity(predef.entityB);
                q = face->FaceGetQuatNum();

                Vector u = q.RotationU();
                Vector v = q.RotationV();

                if(predef.swapUV) swap(u, v);
                if(predef.negateU) u = u.ScaledBy(-1);
                if(predef.negateV) v = v.ScaledBy(-1);
                q = Quaternion::From(u, v);

                origin = face->FaceGetPointNum();
            } else oops();

            Entity normal;
            memset(&normal, 0, sizeof(normal));
            normal.type = Entity::NORMAL_N_COPY;
            normal.numNormal = q;
            normal.point[0] = h.entity(2);
            normal.group = h;
            normal.h = h.entity(1);
            entity->Add(&normal);

            Entity point;
            memset(&point, 0, sizeof(point));
            point.type = Entity::POINT_N_COPY;
            point.numPoint = origin;
            point.group = h;
            point.h = h.entity(2);
            entity->Add(&point);

            Entity wp;
            memset(&wp, 0, sizeof(wp));
            wp.type = Entity::WORKPLANE;
            wp.normal = normal.h;
            wp.point[0] = point.h;
            wp.group = h;
            wp.h = h.entity(0);
            entity->Add(&wp);
            break;
        }

        case EXTRUDE: {
            AddParam(param, h.param(0), gn.x);
            AddParam(param, h.param(1), gn.y);
            AddParam(param, h.param(2), gn.z);
            int ai, af;
            if(subtype == ONE_SIDED) {
                ai = 0; af = 2;
            } else if(subtype == TWO_SIDED) {
                ai = -1; af = 1;
            } else oops();

            // Get some arbitrary point in the sketch, that will be used
            // as a reference when defining top and bottom faces.
            hEntity pt = { 0 };
            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                if(e->IsPoint()) pt = e->h;

                e->CalculateNumerical(false);
                hEntity he = e->h; e = NULL;
                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.
                CopyEntity(entity, SK.GetEntity(he), ai, REMAP_BOTTOM,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);
                CopyEntity(entity, SK.GetEntity(he), af, REMAP_TOP,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);
                MakeExtrusionLines(entity, he);
            }
            // Remapped versions of that arbitrary point will be used to
            // provide points on the plane faces.
            MakeExtrusionTopBottomFaces(entity, pt);
            break;
        }

        case LATHE: {
            Vector axis_pos = SK.GetEntity(predef.origin)->PointGetNum();
            Vector axis_dir = SK.GetEntity(predef.entityB)->VectorGetNum();

            AddParam(param, h.param(0), axis_dir.x);
            AddParam(param, h.param(1), axis_dir.y);
            AddParam(param, h.param(2), axis_dir.z);

            // Remapped entity index.
            int ai = 1;

            for(i = 0; i < entity->n; i++) {
                Entity *e = &(entity->elem[i]);
                if(e->group.v != opA.v) continue;

                e->CalculateNumerical(false);
                hEntity he = e->h;

                // As soon as I call CopyEntity, e may become invalid! That
                // adds entities, which may cause a realloc.
                CopyEntity(entity, SK.GetEntity(predef.origin), 0, ai,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);

                CopyEntity(entity, SK.GetEntity(he), 0, REMAP_LATHE_START,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);

                CopyEntity(entity, SK.GetEntity(he), 0, REMAP_LATHE_END,
                    h.param(0), h.param(1), h.param(2),
                    NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                    true, false);

                MakeLatheCircles(entity, param, he, axis_pos, axis_dir, ai);
                ai++;
            }
            break;
        }

        case TRANSLATE: {
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);

            int n = (int)valA, a0 = 0;
            if(subtype == ONE_SIDED && skipFirst) {
                a0++; n++;
            }

            for(a = a0; a < n; a++) {
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->elem[i]);
                    if(e->group.v != opA.v) continue;

                    e->CalculateNumerical(false);
                    CopyEntity(entity, e,
                        a*2 - (subtype == ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        NO_PARAM, NO_PARAM, NO_PARAM, NO_PARAM,
                        true, false);
                }
            }
            break;
        }
        case ROTATE: {
            // The center of rotation
            AddParam(param, h.param(0), gc.x);
            AddParam(param, h.param(1), gc.y);
            AddParam(param, h.param(2), gc.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 30*PI/180);
            AddParam(param, h.param(4), gn.x);
            AddParam(param, h.param(5), gn.y);
            AddParam(param, h.param(6), gn.z);

            int n = (int)valA, a0 = 0;
            if(subtype == ONE_SIDED && skipFirst) {
                a0++; n++;
            }

            for(a = a0; a < n; a++) {
                for(i = 0; i < entity->n; i++) {
                    Entity *e = &(entity->elem[i]);
                    if(e->group.v != opA.v) continue;

                    e->CalculateNumerical(false);
                    CopyEntity(entity, e,
                        a*2 - (subtype == ONE_SIDED ? 0 : (n-1)),
                        (a == (n - 1)) ? REMAP_LAST : a,
                        h.param(0), h.param(1), h.param(2),
                        h.param(3), h.param(4), h.param(5), h.param(6),
                        false, true);
                }
            }
            break;
        }
        case IMPORTED:
            // The translation vector
            AddParam(param, h.param(0), gp.x);
            AddParam(param, h.param(1), gp.y);
            AddParam(param, h.param(2), gp.z);
            // The rotation quaternion
            AddParam(param, h.param(3), 1);
            AddParam(param, h.param(4), 0);
            AddParam(param, h.param(5), 0);
            AddParam(param, h.param(6), 0);

            for(i = 0; i < impEntity.n; i++) {
                Entity *ie = &(impEntity.elem[i]);
                CopyEntity(entity, ie, 0, 0,
                    h.param(0), h.param(1), h.param(2),
                    h.param(3), h.param(4), h.param(5), h.param(6),
                    false, false);
            }
            break;

        default: oops();
    }
}

void Group::AddEq(IdList<Equation,hEquation> *l, Expr *expr, int index) {
    Equation eq;
    eq.e = expr;
    eq.h = h.equation(index);
    l->Add(&eq);
}

void Group::GenerateEquations(IdList<Equation,hEquation> *l) {
    Equation eq;
    ZERO(&eq);
    if(type == IMPORTED) {
        // Normalize the quaternion
        ExprQuaternion q = {
            Expr::From(h.param(3)),
            Expr::From(h.param(4)),
            Expr::From(h.param(5)),
            Expr::From(h.param(6)) };
        AddEq(l, (q.Magnitude())->Minus(Expr::From(1)), 0);
    } else if(type == ROTATE) {
        // The axis and center of rotation are specified numerically
#define EC(x) (Expr::From(x))
#define EP(x) (Expr::From(h.param(x)))
        ExprVector orig = SK.GetEntity(predef.origin)->PointGetExprs();
        AddEq(l, (orig.x)->Minus(EP(0)), 0);
        AddEq(l, (orig.y)->Minus(EP(1)), 1);
        AddEq(l, (orig.z)->Minus(EP(2)), 2);
        // param 3 is the angle, which is free
        Vector axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);
        AddEq(l, (EC(axis.x))->Minus(EP(4)), 3);
        AddEq(l, (EC(axis.y))->Minus(EP(5)), 4);
        AddEq(l, (EC(axis.z))->Minus(EP(6)), 5);
#undef EC
#undef EP
    } else if(type == EXTRUDE) {
        if(predef.entityB.v != Entity::FREE_IN_3D.v) {
            // The extrusion path is locked along a line, normal to the
            // specified workplane.
            Entity *w = SK.GetEntity(predef.entityB);
            ExprVector u = w->Normal()->NormalExprsU();
            ExprVector v = w->Normal()->NormalExprsV();
            ExprVector extruden = {
                Expr::From(h.param(0)),
                Expr::From(h.param(1)),
                Expr::From(h.param(2)) };

            AddEq(l, u.Dot(extruden), 0);
            AddEq(l, v.Dot(extruden), 1);
        }
    } else if(type == TRANSLATE) {
        if(predef.entityB.v != Entity::FREE_IN_3D.v) {
            Entity *w = SK.GetEntity(predef.entityB);
            ExprVector n = w->Normal()->NormalExprsN();
            ExprVector trans;
            trans = ExprVector::From(h.param(0), h.param(1), h.param(2));

            // The translation vector is parallel to the workplane
            AddEq(l, trans.Dot(n), 0);
        }
    }
}

hEntity Group::Remap(hEntity in, int copyNumber) {
    // A hash table is used to accelerate the search
    int hash = ((unsigned)(in.v*61 + copyNumber)) % REMAP_PRIME;
    int i = remapCache[hash];
    if(i >= 0 && i < remap.n) {
        EntityMap *em = &(remap.elem[i]);
        if(em->input.v == in.v && em->copyNumber == copyNumber) {
            return h.entity(em->h.v);
        }
    }
    // but if we don't find it in the hash table, then linear search
    for(i = 0; i < remap.n; i++) {
        EntityMap *em = &(remap.elem[i]);
        if(em->input.v == in.v && em->copyNumber == copyNumber) {
            // We already have a mapping for this entity.
            remapCache[hash] = i;
            return h.entity(em->h.v);
        }
    }
    // And if we still don't find it, then create a new entry.
    EntityMap em;
    em.input = in;
    em.copyNumber = copyNumber;
    remap.AddAndAssignId(&em);
    return h.entity(em.h.v);
}

void Group::MakeExtrusionLines(IdList<Entity,hEntity> *el, hEntity in) {
    Entity *ep = SK.GetEntity(in);

    // Get workplane of the sketch group.
    Entity *w = SK.GetEntity(predef.entityB);

    // Let the face's u coordinate be the workplane normal.
    Vector u = w->Normal()->numNormal.RotationN();

    Entity en;
    ZERO(&en);
    if(ep->IsPoint()) {
        // A point gets extruded to form a line segment
        en.point[0] = Remap(ep->h, REMAP_TOP);
        en.point[1] = Remap(ep->h, REMAP_BOTTOM);
        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_PT_TO_LINE);
        en.type = Entity::LINE_SEGMENT;
        el->Add(&en);
    } else if(ep->type == Entity::LINE_SEGMENT) {
        // A line gets extruded to form a plane face; an endpoint of the
        // original line is a point in the plane, and the line is in the plane.
        Vector a = SK.GetEntity(ep->point[0])->PointGetNum();
        Vector b = SK.GetEntity(ep->point[1])->PointGetNum();
        Vector v = b.Minus(a).WithMagnitude(1.0);

        en.numPoint = a;
        en.numNormal = Quaternion::From(u, v);

        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_LINE_TO_FACE);
        en.type = Entity::FACE_QUAT_PT;
        el->Add(&en);
    }
}

void Group::MakeLatheCircles(IdList<Entity,hEntity> *el, IdList<Param,hParam> *param, hEntity in, Vector pt, Vector axis, int ai) {
    Entity *ep = SK.GetEntity(in);

    Entity en;
    ZERO(&en);

    if(ep->IsPoint()) {
        // A point gets revolved to form an arc.
        en.point[0] = Remap(predef.origin, ai);
        en.point[1] = Remap(ep->h, REMAP_LATHE_START);
        en.point[2] = Remap(ep->h, REMAP_LATHE_END);

        // Get arc center and point on arc.
        Entity *pc = SK.GetEntity(en.point[0]);
        Entity *pp = SK.GetEntity(en.point[1]);

        // Project arc point to the revolution axis and use it for arc center.
        double k = pp->numPoint.Minus(pt).Dot(axis) / axis.Dot(axis);
        pc->numPoint = pt.Plus(axis.ScaledBy(k));

        // Create arc entity.
        en.group = h;
        en.construction = ep->construction;
        en.style = ep->style;
        en.h = Remap(ep->h, REMAP_PT_TO_ARC);
        en.type = Entity::ARC_OF_CIRCLE;

        // Generate a normal.
        Entity n;
        memset(&n, 0, sizeof(n));
        n.workplane = en.workplane;
        n.h = Remap(ep->h, REMAP_PT_TO_NORMAL);
        n.group = en.group;
        n.style = en.style;
        n.type = Entity::NORMAL_N_COPY;

        // Create basis for the normal.
        Vector nu = pp->numPoint.Minus(pc->numPoint).WithMagnitude(1.0);
        Vector nv = nu.Cross(axis).WithMagnitude(1.0);
        n.numNormal = Quaternion::From(nv, nu);

        // The point determines where the normal gets displayed on-screen;
        // it's entirely cosmetic.
        n.point[0] = en.point[0];
        el->Add(&n);
        en.normal = n.h;
        el->Add(&en);
    } else if(ep->type == Entity::LINE_SEGMENT) {
        // An axis-perpendicular line gets revolved to form a face.
        Vector a = SK.GetEntity(ep->point[0])->PointGetNum();
        Vector b = SK.GetEntity(ep->point[1])->PointGetNum();
        Vector u = b.Minus(a).WithMagnitude(1.0);

        // Check for perpendicularity: calculate cosine of the angle
        // between axis and line direction and check that
        // cos(angle) == 0 <-> angle == +-90 deg.
        if(fabs(u.Dot(axis) / axis.Magnitude()) < ANGLE_COS_EPS) {
            en.param[0] = h.param(0);
            en.param[1] = h.param(1);
            en.param[2] = h.param(2);
            Vector v = axis.Cross(u).WithMagnitude(1.0);
            en.numNormal = Quaternion::From(u, v);

            en.group = h;
            en.construction = ep->construction;
            en.style = ep->style;
            en.h = Remap(ep->h, REMAP_LINE_TO_FACE);
            en.type = Entity::FACE_QUAT_PT;
            en.numPoint = a;
            el->Add(&en);
        }
    }
}

void Group::MakeExtrusionTopBottomFaces(IdList<Entity,hEntity> *el, hEntity pt)
{
    if(pt.v == 0) return;
    Group *src = SK.GetGroup(opA);
    Vector n = src->polyLoops.normal;

    Entity en;
    ZERO(&en);
    en.type = Entity::FACE_QUAT_PT;
    en.group = h;

    Entity *w = SK.GetEntity(predef.entityB);
    en.numNormal = w->Normal()->numNormal;
    en.numPoint = SK.GetEntity(pt)->PointGetNum();
    en.h = Remap(Entity::NO_ENTITY, REMAP_TOP);
    el->Add(&en);

    en.point[0] = Remap(pt, REMAP_BOTTOM);
    en.h = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
    el->Add(&en);
}

void Group::CopyEntity(IdList<Entity,hEntity> *el,
                       Entity *ep, int timesApplied, int remap,
                       hParam dx, hParam dy, hParam dz,
                       hParam qw, hParam qvx, hParam qvy, hParam qvz,
                       bool asTrans, bool asAxisAngle)
{
    Entity en;
    ZERO(&en);
    en.type = ep->type;
    en.extraPoints = ep->extraPoints;
    en.h = Remap(ep->h, remap);
    en.timesApplied = timesApplied;
    en.group = h;
    en.construction = ep->construction;
    en.style = ep->style;
    en.str.strcpy(ep->str.str);
    en.font.strcpy(ep->font.str);

    switch(ep->type) {
        case Entity::WORKPLANE:
            // Don't copy these.
            return;

        case Entity::POINT_N_COPY:
        case Entity::POINT_N_TRANS:
        case Entity::POINT_N_ROT_TRANS:
        case Entity::POINT_N_ROT_AA:
        case Entity::POINT_IN_3D:
        case Entity::POINT_IN_2D:
            if(asTrans) {
                en.type = Entity::POINT_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else {
                if(asAxisAngle) {
                    en.type = Entity::POINT_N_ROT_AA;
                } else {
                    en.type = Entity::POINT_N_ROT_TRANS;
                }
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
            }
            en.numPoint = (ep->actPoint).ScaledBy(scale);
            break;

        case Entity::NORMAL_N_COPY:
        case Entity::NORMAL_N_ROT:
        case Entity::NORMAL_N_ROT_AA:
        case Entity::NORMAL_IN_3D:
        case Entity::NORMAL_IN_2D:
            if(asTrans) {
                en.type = Entity::NORMAL_N_COPY;
            } else {
                if(asAxisAngle) {
                    en.type = Entity::NORMAL_N_ROT_AA;
                } else {
                    en.type = Entity::NORMAL_N_ROT;
                }
                en.param[0] = qw;
                en.param[1] = qvx;
                en.param[2] = qvy;
                en.param[3] = qvz;
            }
            en.numNormal = ep->actNormal;
            if(scale < 0) en.numNormal = en.numNormal.Mirror();

            en.point[0] = Remap(ep->point[0], remap);
            break;

        case Entity::DISTANCE_N_COPY:
        case Entity::DISTANCE:
            en.type = Entity::DISTANCE_N_COPY;
            en.numDistance = ep->actDistance*fabs(scale);
            break;

        case Entity::FACE_QUAT_PT:
        case Entity::FACE_N_ROT_TRANS:
        case Entity::FACE_N_TRANS:
        case Entity::FACE_N_ROT_AA:
            if(asTrans) {
                en.type = Entity::FACE_N_TRANS;
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
            } else {
                if(asAxisAngle) {
                    en.type = Entity::FACE_N_ROT_AA;
                } else {
                    en.type = Entity::FACE_N_ROT_TRANS;
                }
                en.param[0] = dx;
                en.param[1] = dy;
                en.param[2] = dz;
                en.param[3] = qw;
                en.param[4] = qvx;
                en.param[5] = qvy;
                en.param[6] = qvz;
            }
            en.numPoint  = (ep->actPoint).ScaledBy(scale);
            en.numNormal = (ep->actNormal).ScaledBy(scale);
            break;

        default: {
            int i, points;
            bool hasNormal, hasDistance;
            EntReqTable::GetEntityInfo(ep->type, ep->extraPoints,
                NULL, &points, &hasNormal, &hasDistance);
            for(i = 0; i < points; i++) {
                en.point[i] = Remap(ep->point[i], remap);
            }
            if(hasNormal)   en.normal   = Remap(ep->normal, remap);
            if(hasDistance) en.distance = Remap(ep->distance, remap);
            break;
        }
    }

    // If the entity came from an imported file where it was invisible then
    // ep->actiVisble will be false, and we should hide it. Or if the entity
    // came from a copy (e.g. step and repeat) of a force-hidden imported
    // entity, then we also want to hide it.
    en.forceHidden = (!ep->actVisible) || ep->forceHidden;

    el->Add(&en);
}

