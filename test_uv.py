def generate_uvs():
    # v0, v1, v2, v3
    # v1 = v0 + du
    # v3 = v0 + dv
    # v2 = v0 + du + dv
    
    # dimensions:
    # d=0 (X): u=1(Y, len w), v=2(Z, len h)
    # dir=1 (+X): Up=+Y(du), Right=+Z(dv)
    # v0=BL(0,w), v1=TL(0,0), v2=TR(h,0), v3=BR(h,w)
    
    # dir=2 (-X): Up=+Y(du), Right=-Z(-dv) => Left=+Z(dv)
    # v0=BR(h,w), v1=TR(h,0), v2=TL(0,0), v3=BL(0,w)
    
    # d=1 (Y): u=2(Z, len w), v=0(X, len h)
    # dir=1 (+Y): Up=-Z(-du), Right=+X(dv)  (Standard top-down view: North(-Z) is Up, East(+X) is Right)
    # Wait, if Up=-du, Down=+du.
    # v0=TL(0,0) (since du is Down and dv is Right)
    # v1=BL(0,w)
    # v3=TR(h,0)
    # v2=BR(h,w)
    
    # dir=2 (-Y): Up=+Z(du), Right=+X(dv) (Looking from bottom up: South(+Z) is Up, East(+X) is Right)
    # v0=BL(0,w)
    # v1=TL(0,0)
    # v3=BR(h,w)
    # v2=TR(h,0)
    
    # d=2 (Z): u=0(X, len w), v=1(Y, len h)
    # dir=1 (+Z, Front face): Up=+Y(dv), Right=-X(-du) => Left=+X(du)
    # v0=BR(w,h)
    # v1=BL(0,h)
    # v3=TR(w,0)
    # v2=TL(0,0)
    
    # dir=2 (-Z, Back face): Up=+Y(dv), Right=+X(du)
    # v0=BL(0,h)
    # v1=BR(w,h)
    # v3=TL(0,0)
    # v2=TR(w,0)
    pass
generate_uvs()
