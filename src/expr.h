//-----------------------------------------------------------------------------
// An expression in our symbolic algebra system, used to write, linearize,
// and solve our constraint equations.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------

#ifndef __EXPR_H
#define __EXPR_H

class Expr;
class Expr : public std::enable_shared_from_this<Expr> {
public:
    uint32_t marker;

    enum {
        // A parameter, by the hParam handle
        PARAM          =  0,
        // A parameter, by a pointer straight in to the param table (faster,
        // if we know that the param table won't move around)
        PARAM_PTR      =  1,

        // These are used only for user-entered expressions.
        POINT          = 10,
        ENTITY         = 11,

        CONSTANT       = 20,

        PLUS           = 100,
        MINUS          = 101,
        TIMES          = 102,
        DIV            = 103,
        NEGATE         = 104,
        SQRT           = 105,
        SQUARE         = 106,
        SIN            = 107,
        COS            = 108,
        ASIN           = 109,
        ACOS           = 110,

        // Special helpers for when we're parsing an expression from text.
        // Initially, literals (like a constant number) appear in the same
        // format as they will in the finished expression, but the operators
        // are different until the parser fixes things up (and builds the
        // tree from the flat list that the lexer outputs).
        ALL_RESOLVED   = 1000,
        PAREN          = 1001,
        BINARY_OP      = 1002,
        UNARY_OP       = 1003
    };

    int op;
    ExprRef a;
    ExprRef b;
    union {
        double  v;
        hParam  parh;
        Param  *parp;
        hEntity entity;

        // For use while parsing
        char    c;
    }       x;

    static ExprRef From(hParam p);
    static ExprRef From(double v);

    ExprRef AnyOp(int op, ExprRef b);
    inline ExprRef Plus (ExprRef b_) { return AnyOp(PLUS,  b_); }
    inline ExprRef Minus(ExprRef b_) { return AnyOp(MINUS, b_); }
    inline ExprRef Times(ExprRef b_) { return AnyOp(TIMES, b_); }
    inline ExprRef Div  (ExprRef b_) { return AnyOp(DIV,   b_); }

    inline ExprRef Negate(void) { return AnyOp(NEGATE, NULL); }
    inline ExprRef Sqrt  (void) { return AnyOp(SQRT,   NULL); }
    inline ExprRef Square(void) { return AnyOp(SQUARE, NULL); }
    inline ExprRef Sin   (void) { return AnyOp(SIN,    NULL); }
    inline ExprRef Cos   (void) { return AnyOp(COS,    NULL); }
    inline ExprRef ASin  (void) { return AnyOp(ASIN,   NULL); }
    inline ExprRef ACos  (void) { return AnyOp(ACOS,   NULL); }

    ExprRef PartialWrt(hParam p);
    double Eval(void);
    uint64_t ParamsUsed(void);
    bool DependsOn(hParam p);
    static bool Tol(double a, double b);
    ExprRef FoldConstants(void);
    void Substitute(hParam oldh, hParam newh);

    static const hParam NO_PARAMS, MULTIPLE_PARAMS;
    hParam ReferencedParams(ParamList *pl);

    void ParamsToPointers(void);

    void App(const char *str, ...);
    const char *Print(void);
    void PrintW(void); // worker

    // number of child nodes: 0 (e.g. constant), 1 (sqrt), or 2 (+)
    int Children(void);
    // total number of nodes in the tree
    int Nodes(void);

    // Make a simple copy
    ExprRef DeepCopy(void);
    // Make a copy, with the parameters (usually referenced by hParam)
    // resolved to pointers to the actual value. This speeds things up
    // considerably.
    ExprRef DeepCopyWithParamsAsPointers(IdList<Param,hParam> *firstTry,
        IdList<Param,hParam> *thenTry);

    static ExprRef From(const char *in, bool popUpError);
    static void  Lex(const char *in);
    static ExprRef Next(void);
    static void  Consume(void);

    static void PushOperator(ExprRef e);
    static ExprRef PopOperator(void);
    static ExprRef TopOperator(void);
    static void PushOperand(ExprRef e);
    static ExprRef PopOperand(void);

    static void Reduce(void);
    static void ReduceAndPush(ExprRef e);
    static int Precedence(ExprRef e);

    static int Precedence(int op);
    static void Parse(void);
};

class ExprVector {
public:
    ExprRef x, y, z;

    static ExprVector From(ExprRef x, ExprRef y, ExprRef z);
    static ExprVector From(Vector vn);
    static ExprVector From(hParam x, hParam y, hParam z);
    static ExprVector From(double x, double y, double z);

    ExprVector Plus(ExprVector b);
    ExprVector Minus(ExprVector b);
    ExprRef Dot(ExprVector b);
    ExprVector Cross(ExprVector b);
    ExprVector ScaledBy(ExprRef s);
    ExprVector WithMagnitude(ExprRef s);
    ExprRef Magnitude(void);

    Vector Eval(void);
};

class ExprQuaternion {
public:
    ExprRef w, vx, vy, vz;

    static ExprQuaternion From(ExprRef w, ExprRef vx, ExprRef vy, ExprRef vz);
    static ExprQuaternion From(Quaternion qn);
    static ExprQuaternion From(hParam w, hParam vx, hParam vy, hParam vz);

    ExprVector RotationU(void);
    ExprVector RotationV(void);
    ExprVector RotationN(void);

    ExprVector Rotate(ExprVector p);
    ExprQuaternion Times(ExprQuaternion b);

    ExprRef Magnitude(void);
};

#endif

