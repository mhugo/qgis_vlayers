# -*- coding: utf-8 -*-

# keywords
keywords = [
    # TODO get them from a reference page
    "action", "add", "after", "all", "alter", "analyze", "and", "as", "asc",
    "before", "begin", "between", "by", "cascade", "case", "cast", "check",
    "collate", "column", "commit", "constraint", "create", "cross", "current_date",
    "current_time", "current_timestamp", "default", "deferrable", "deferred",
    "delete", "desc", "distinct", "drop", "each", "else", "end", "escape",
    "except", "exists", "for", "foreign", "from", "full", "group", "having",
    "ignore", "immediate", "in", "initially", "inner", "insert", "intersect",
    "into", "is", "isnull", "join", "key", "left", "like", "limit", "match",
    "natural", "no", "not", "notnull", "null", "of", "offset", "on", "or", "order",
    "outer", "primary", "references", "release", "restrict", "right", "rollback",
    "row", "savepoint", "select", "set", "table", "temporary", "then", "to",
    "transaction", "trigger", "union", "unique", "update", "using", "values",
    "view", "when", "where",

    "abort", "attach", "autoincrement", "conflict", "database", "detach",
    "exclusive", "explain", "fail", "glob", "if", "index", "indexed", "instead",
    "plan", "pragma", "query", "raise", "regexp", "reindex", "rename", "replace",
    "temp", "vacuum", "virtual"
]
spatialite_keywords = []

# functions
functions = [
    # TODO get them from a reference page
    "changes", "coalesce", "glob", "ifnull", "hex", "last_insert_rowid",
    "nullif", "quote", "random",
    "randomblob", "replace", "round", "soundex", "total_change", 
    "typeof", "zeroblob", "date", "datetime", "julianday", "strftime"
]
operators=[
' AND ',' OR ','||',' < ',' <= ',' > ',' >= ',' = ',' <> ',' IS ',' IS NOT ',' IN ',' LIKE ',' GLOB ',' MATCH ',' REGEXP '
]

math_functions = [
    # SQL math functions
    "Abs", "ACos", "ASin", "ATan", "Cos", "Cot", "Degrees", "Exp", "Floor", "Log", "Log2",
    "Log10", "Pi", "Radians", "Round", "Sign", "Sin", "Sqrt", "StdDev_Pop", "StdDev_Samp", "Tan",
    "Var_Pop", "Var_Samp" ]

string_functions=["Length", "Lower", "Upper", "Like", "Trim", "LTrim", "RTrim", "Replace", "Substr"]

aggregate_functions=[
"Max","Min","Avg","Count","Sum","Group_Concat","Total","Var_Pop","Var_Samp","StdDev_Pop","StdDev_Samp"
]

spatialite_functions = [  # from www.gaia-gis.it/spatialite-2.3.0/spatialite-sql-2.3.0.html
                          # SQL utility functions for BLOB objects
                          "*iszipblob", "*ispdfblob", "*isgifblob", "*ispngblob", "*isjpegblob", "*isexifblob",
                          "*isexifgpsblob", "*geomfromexifgpsblob", "MakePoint", "buildmbr", "*buildcirclembr", "MbrMinX",
                          "MbrMinY", "MbrMaxX", "MbrMaxY",
                          # SQL functions for constructing a geometric object given its Well-known Text Representation
                          "GeomFromText", "*pointfromtext",
                          # SQL functions for constructing a geometric object given its Well-known Binary Representation
                          "*geomfromwkb", "*pointfromwkb",
                          # SQL functions for obtaining the Well-known Text / Well-known Binary Representation of a geometric object
                          "AsText", "AsBinary",
                          # SQL functions supporting exotic geometric formats
                          "*assvg", "*asfgf", "*geomfromfgf",
                          # SQL functions on type Geometry
                          "Dimension", "GeometryType", "Srid", "SetSrid", "isEmpty", "isSimple", "isValid", "Boundary",
                          "Envelope",
                          # SQL functions on type Point
                          "X", "Y",
                          # SQL functions on type Curve [Linestring or Ring]
                          "StartPoint", "EndPoint", "GLength", "isClosed", "isRing", "Simplify",
                          "*simplifypreservetopology",
                          # SQL functions on type LineString
                          "NumPoints", "PointN",
                          # SQL functions on type Surface [Polygon or Ring]
                          "Centroid", "PointOnSurface", "Area",
                          # SQL functions on type Polygon
                          "ExteriorRing", "InteriorRingN",
                          # SQL functions on type GeomCollection
                          "NumGeometries", "GeometryN",
                          # SQL functions that test approximative spatial relationships via MBRs
                          "MbrEqual", "MbrDisjoint", "MbrTouches", "MbrWithin", "MbrOverlaps", "MbrIntersects",
                          "MbrContains",
                          # SQL functions that test spatial relationships
                          "Equals", "Disjoint", "Touches", "Within", "Overlaps", "Crosses", "Intersects", "Contains",
                          "Relate",
                          # SQL functions for distance relationships
                          "Distance",
                          # SQL functions that implement spatial operators
                          "Intersection", "Difference", "GUnion", "SymDifference", "Buffer", "ConvexHull",
                          # SQL functions for coordinate transformations
                          "Transform",
                          # SQL functions for Spatial-MetaData and Spatial-Index handling
                          "*initspatialmetadata", "*addgeometrycolumn", "*recovergeometrycolumn", "*discardgeometrycolumn",
                          "*createspatialindex", "*creatembrcache", "*disablespatialindex",
                          # SQL functions implementing FDO/OGR compatibily
                          "*checkspatialmetadata", "*autofdostart", "*autofdostop", "*initfdospatialmetadata",
                          "*addfdogeometrycolumn", "*recoverfdogeometrycolumn", "*discardfdogeometrycolumn",
                          # SQL functions for MbrCache-based queries
                          "*filtermbrwithin", "*filtermbrcontains", "*filtermbrintersects", "*buildmbrfilter"
]

# constants
constants = ["null", "false", "true"]
spatialite_constants = []

def getSqlDictionary(spatial=True):
    def strip_star(s):
        if s[0] == '*':
            return s.lower()[1:]
        else:
            return s.lower()

    k, c, f = list(keywords), list(constants), list(functions)

    if spatial:
        k += spatialite_keywords
        f += spatialite_functions
        c += spatialite_constants

    return {'keyword': map(strip_star,k), 'constant': map(strip_star,c), 'function': map(strip_star,f)}

def getQueryBuilderDictionary():
    # concat functions
    def ff( l ):
        return filter( lambda s:s[0] != '*', l )
    def add_paren( l ):
        return map( lambda s:s+"(", l )
    foo = sorted(add_paren(ff( list(set.union(set(functions), set(spatialite_functions))) )))
    m = sorted(add_paren(ff( math_functions )))
    agg = sorted(add_paren(ff(aggregate_functions)))
    op = ff(operators)
    s = sorted(add_paren(ff(string_functions)))
    return {'function': foo, 'math' : m, 'aggregate': agg, 'operator': op, 'string': s }
