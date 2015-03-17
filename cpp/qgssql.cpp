#include <qgssql.h>
#include <qgsfeature.h>

#include <QSet>

namespace QgsSql
{

void Node::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void List::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void Expression::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionLiteral::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionUnaryOperator::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionBinaryOperator::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionFunction::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionCondition::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionWhenThen::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionIn::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void TableColumn::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void TableName::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void TableSelect::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ColumnExpression::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void AllColumns::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void JoinedTable::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void Select::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void CompoundSelect::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void Having::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void GroupBy::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void LimitOffset::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void OrderingTerm::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void OrderBy::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void SelectStmt::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionSubQuery::accept( NodeVisitor& v ) const {
    v.visit( *this );
}
void ExpressionCast::accept( NodeVisitor& v ) const {
    v.visit( *this );
}



void DFSVisitor::visit( const Select& s )
{
    if ( s.column_list() ) {
        s.column_list()->accept( *this );
    }
    if ( s.from() ) {
        s.from()->accept( *this );
    }
    if ( s.where() ) {
        s.where()->accept(*this);
    }
}

void DFSVisitor::visit( const SelectStmt& s )
{
    if ( s.selects() ) { s.selects()->accept(*this); }
    if ( s.order_by() ) { s.order_by()->accept(*this); }
    if ( s.limit_offset() ) { s.limit_offset()->accept(*this); }
}

void DFSVisitor::visit( const OrderBy& s )
{
    if ( s.terms() ) { s.terms()->accept(*this); }
}

void DFSVisitor::visit( const LimitOffset& s )
{
    if ( s.limit() ) { s.limit()->accept(*this); }
    if ( s.offset() ) { s.offset()->accept(*this); }
}

void DFSVisitor::visit( const CompoundSelect& s )
{
    if ( s.select() ) { s.select()->accept(*this); }
}

void DFSVisitor::visit( const List& l )
{
    for ( auto& n: l ) {
        n->accept(*this);
    }
}

void DFSVisitor::visit( const TableSelect& s )
{
    if ( s.select() ) {
        s.select()->accept(*this);
    }
}

void DFSVisitor::visit( const JoinedTable& jt )
{
    if ( jt.right_table() ) {
        jt.right_table()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionUnaryOperator& op )
{
    if ( op.expression() ) {
        op.expression()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionBinaryOperator& op )
{
    if ( op.left() ) {
        op.left()->accept(*this);
    }
    if ( op.right() ) {
        op.right()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionFunction& f )
{
    if (f.args()) {
        f.args()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionCondition& c )
{
    if (c.conditions()) {
        c.conditions()->accept(*this);
    }
    if (c.else_node()) {
        c.else_node()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionWhenThen& wt )
{
    if (wt.when()) {
        wt.when()->accept(*this);
    }
    if (wt.then_node()) {
        wt.then_node()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionIn& t )
{
    if (t.expression()) { t.expression()->accept(*this); }
    if (t.in_what()) { t.in_what()->accept(*this); }
}

void DFSVisitor::visit(const ExpressionSubQuery& t )
{
    if (t.select()) { t.select()->accept(*this); }
}

void DFSVisitor::visit(const ExpressionCast& t )
{
    if (t.expression()) { t.expression()->accept(*this); }
}

void DFSVisitor::visit(const ColumnExpression& t )
{
    if (t.expression()) { t.expression()->accept(*this); }
}

class AsStringVisitor : public DFSVisitor
{
public:
    virtual void visit( const Select& s ) override
    {
        str_ += "SELECT ";
        s.column_list()->accept( *this );
        if ( s.from() ) {
            str_ += " FROM ";
            s.from()->accept( *this );
        }
        if ( s.where() ) {
            str_ += " WHERE ";
            s.where()->accept( *this );
        }
    }
    virtual void visit( const List& l ) override
    {
        str_ += "[";
        bool first = true;
        for ( auto& n: l ) {
            if ( first ) {
                first = false;
            }
            else {
                str_ += ", ";
            }
            n->accept(*this);
        }
        str_ += "]";
    }
    virtual void visit( const TableColumn& t ) override
    {
        str_ += t.table() + "." + t.column();
    }
    virtual void visit( const TableName& t ) override
    {
        str_ += t.name() + " AS " + t.alias();
    }
    virtual void visit( const ExpressionLiteral& t ) override
    {
        str_ += t.value().toString();
    }
    virtual void visit( const ExpressionIn& t ) override
    {
        t.expression()->accept(*this);
        if (t.not_in()) {
            str_ += " NOT ";
        }
        str_ += " IN ";
        t.in_what()->accept(*this);
    }
    QString str_;
};

QString asString( const Node& n )
{
    AsStringVisitor pv;
    n.accept(pv);
    return pv.str_;
}

class TableVisitor : public DFSVisitor
{
public:
    TableVisitor() : DFSVisitor() {}

    // collect table names over the query
    virtual void visit( const TableName& t ) override
    {
        tables.insert( t.name() );
    }

    QSet<QString> tables;
};

QList<QString> referencedTables( const Node& n )
{
    TableVisitor tv;
    n.accept(tv);
    return tv.tables.toList();
}

class InfererException
{
public:
    InfererException( const QString& err ) : err_(err) {}
    QString err() const { return err_; }
private:
    QString err_;
};

QList<ColumnDef> columnTypes_r( const Node& n, const TableDefs* tableContext );

QVariant::Type resultingArithmeticType( QVariant::Type ta, QVariant::Type tb )
{
    switch (ta) {
    case QVariant::UserType:
        {
            switch (tb) {
            case QVariant::UserType:
                return QVariant::Int;
            case QVariant::Int:
                return QVariant::Int;
            case QVariant::Double:
                return QVariant::Double;
            case QVariant::String:
                return QVariant::Int;
            }
        }
    case QVariant::Int:
        {
            switch (tb) {
            case QVariant::UserType:
                return QVariant::Int;
            case QVariant::Int:
                return QVariant::Int;
            case QVariant::Double:
                return QVariant::Double;
            case QVariant::String:
                return QVariant::Int;
            }
        }
    case QVariant::Double:
        {
            switch (tb) {
            case QVariant::UserType:
                return QVariant::Double;
            case QVariant::Int:
                return QVariant::Double;
            case QVariant::Double:
                return QVariant::Double;
            case QVariant::String:
                return QVariant::Double;
            }
        }
    case QVariant::String:
        {
            switch (tb) {
            case QVariant::UserType:
                return QVariant::Int;
            case QVariant::Int:
                return QVariant::Int;
            case QVariant::Double:
                return QVariant::Double;
            case QVariant::String:
                return QVariant::Int;
            }
        }
    }
    return QVariant::Int;
}


struct FunctionResult {
    QVariant::Type type;
    int wkbType;
    int sridParameter;
};

class OutputFunctionTypes : public QHash<QString, FunctionResult>
{
 public:
    OutputFunctionTypes() : QHash<QString, FunctionResult>() {}
    virtual ~OutputFunctionTypes() {}

    void add( QString k, QVariant::Type t )
    {
        insert( k, {t, -1, -1} );
    }
    void add( QString k, int wkbType, int sridParameter = -1 ) {
        insert( k, {QVariant::UserType, wkbType, sridParameter} );
    }
};

OutputFunctionTypes initOutputFunctionTypes()
{
    std::cout << "initOutputFunctionTypes" << std::endl;
    OutputFunctionTypes t;
    t.add( "casttointeger1", QVariant::Int );
    t.add( "casttodouble1", QVariant::Double );
    t.add( "casttotext1", QVariant::String );
    t.add( "st_point2", 1 );
    t.add( "makepoint2", 1 );
    t.add( "makepoint3", 1001, 2 );
    t.add( "makepointz3", 1001 );
    t.add( "makepointz4", 1001, 3 );
    t.add( "makepointm3", 2001 );
    t.add( "makepointm4", 2001, 3 );
    t.add( "makepointzm4", 3001 );
    t.add( "makepointzm5", 3001, 4 );
    t.add( "makeline1", 2 ); // aggregate
    t.add( "makeline2", 2 );
    t.add( "pointfromtext1", 1 );
    t.add( "pointfromtext2", 1, 1 );
    t.add( "st_pointfromtext1", 1 );
    t.add( "st_pointfromtext2", 1, 1 );

    t.add( "linefromtext1", 2 );
    t.add( "linefromtext2", 2, 1 );
    t.add( "linestringfromtext1", 2 );
    t.add( "linestringfromtext2", 2, 1 );
    t.add( "st_linefromtext1", 2 );
    t.add( "st_linefromtext2", 2, 1 );
    t.add( "st_linestringfromtext1", 2 );
    t.add( "st_linestringfromtext2", 2, 1 );

    t.add( "polyfromtext1", 3 );
    t.add( "polyfromtext2", 3, 1 );
    t.add( "polygonfromtext1", 3 );
    t.add( "polygonfromtext2", 3, 1 );
    t.add( "st_polyfromtext1", 3 );
    t.add( "st_polyfromtext2", 3, 1 );
    t.add( "st_polygonfromtext1", 3 );
    t.add( "st_polygonfromtext2", 3, 1 );

    t.add( "mpointfromtext1", 4 );
    t.add( "mpointfromtext2", 4, 1 );
    t.add( "st_mpointfromtext1", 4 );
    t.add( "st_mpointfromtext2", 4, 1 );
    t.add( "multipointfromtext1", 4 );
    t.add( "multipointfromtext2", 4, 1 );
    t.add( "st_multipointfromtext1", 4 );
    t.add( "st_multipointfromtext2", 4, 1 );

    t.add( "mlinefromtext1", 5 );
    t.add( "mlinefromtext2", 5, 1 );
    t.add( "multilinestringfromtext1", 5 );
    t.add( "multilinestringfromtext2", 5, 1 );
    t.add( "st_mlinefromtext1", 5 );
    t.add( "st_mlinefromtext2", 5, 1 );
    t.add( "st_multilinestringfromtext1", 5 );
    t.add( "st_multilinestringfromtext2", 5, 1 );

    t.add( "mpolyfromtext1", 6 );
    t.add( "mpolyfromtext2", 6, 1 );
    t.add( "multipolygonfromtext1", 6 );
    t.add( "multipolygonfromtext2", 6, 1 );
    t.add( "st_mpolyfromtext1", 6 );
    t.add( "st_mpolyfromtext2", 6, 1 );
    t.add( "st_multipolygonfromtext1", 6 );
    t.add( "st_multipolygonfromtext2", 6, 1 );

    t.add( "pointfromwkb1", 1 );
    t.add( "pointfromwkb2", 1, 1 );
    t.add( "st_pointfromwkb1", 1 );
    t.add( "st_pointfromwkb2", 1, 1 );

    t.add( "linefromwkb1", 2 );
    t.add( "linefromwkb2", 2, 1 );
    t.add( "linestringfromwkb1", 2 );
    t.add( "linestringfromwkb2", 2, 1 );
    t.add( "st_linefromwkb1", 2 );
    t.add( "st_linefromwkb2", 2, 1 );
    t.add( "st_linestringfromwkb1", 2 );
    t.add( "st_linestringfromwkb2", 2, 1 );

    t.add( "polyfromwkb1", 3 );
    t.add( "polyfromwkb2", 3, 1 );
    t.add( "polygonfromwkb1", 3 );
    t.add( "polygonfromwkb2", 3, 1 );
    t.add( "st_polyfromwkb1", 3 );
    t.add( "st_polyfromwkb2", 3, 1 );
    t.add( "st_polygonfromwkb1", 3 );
    t.add( "st_polygonfromwkb2", 3, 1 );

    t.add( "mpointfromwkb1", 4 );
    t.add( "mpointfromwkb2", 4, 1 );
    t.add( "st_mpointfromwkb1", 4 );
    t.add( "st_mpointfromwkb2", 4, 1 );
    t.add( "multipointfromwkb1", 4 );
    t.add( "multipointfromwkb2", 4, 1 );
    t.add( "st_multipointfromwkb1", 4 );
    t.add( "st_multipointfromwkb2", 4, 1 );

    t.add( "mlinefromwkb1", 5 );
    t.add( "mlinefromwkb2", 5, 1 );
    t.add( "multilinestringfromwkb1", 5 );
    t.add( "multilinestringfromwkb2", 5, 1 );
    t.add( "st_mlinefromwkb1", 5 );
    t.add( "st_mlinefromwkb2", 5, 1 );
    t.add( "st_multilinestringfromwkb1", 5 );
    t.add( "st_multilinestringfromwkb2", 5, 1 );

    t.add( "mpolyfromwkb1", 6 );
    t.add( "mpolyfromwkb2", 6, 1 );
    t.add( "multipolygonfromwkb1", 6 );
    t.add( "multipolygonfromwkb2", 6, 1 );
    t.add( "st_mpolyfromwkb1", 6 );
    t.add( "st_mpolyfromwkb2", 6, 1 );
    t.add( "st_multipolygonfromwkb1", 6 );
    t.add( "st_multipolygonfromwkb2", 6, 1 );

    t.add( "envelope1", 3 );
    t.add( "st_envelope1", 3 );
    t.add( "reverse1", 2 );
    t.add( "st_reverse1", 2 );
    t.add( "forcelhr", 3 );
    t.add( "st_forcelhr", 3 );

    t.add( "casttopoint1", 1 );
    t.add( "casttolinestring1", 2 );
    t.add( "casttopolygon1", 3 );
    t.add( "casttomultipoint1", 4 );
    t.add( "casttomultilinestring1", 5 );
    t.add( "casttomultipolygon1", 6 );

    t.add( "startpoint1", 1 );
    t.add( "st_startpoint1", 1 );
    t.add( "endpoint1", 1 );
    t.add( "st_endpoint1", 1 );
    t.add( "pointonsurface1", 1 );
    t.add( "st_pointonsurface1", 1 );
    t.add( "pointn1", 1 );
    t.add( "st_pointn1", 1 );
    t.add( "addpoint2", 2 );
    t.add( "addpoint3", 2 );
    t.add( "st_addpoint2", 2 );
    t.add( "st_addpoint3", 2 );
    t.add( "setpoint3", 2 );
    t.add( "st_setpoint3", 2 );
    t.add( "removepoint2", 2 );
    t.add( "st_removepoint2", 2 );

    t.add( "centroid1", 1 );
    t.add( "st_centroid1", 1 );

    t.add( "exteriorring1", 2 );
    t.add( "st_exteriorring1", 2 );
    t.add( "interiorringn2", 2 );
    t.add( "st_interiorringn2", 2 );

    t.add( "buffer2", 3 );
    t.add( "st_buffer2", 3 );
    t.add( "convexhull1", 3 );
    t.add( "st_convexhull1", 3 );
    t.add( "line_interpolate_point2", 1 );
    t.add( "st_line_interpolate_point2", 1 );
    t.add( "line_interpolate_equidistant_points2", 4 );
    t.add( "st_line_interpolate_equidistant_points2", 4 );
    t.add( "line_substring3", 2 );
    t.add( "st_line_substring3", 2 );
    t.add( "closestpoint2", 1 );
    t.add( "st_closestpoint2", 1 );
    t.add( "shortestline2", 2 );
    t.add( "st_shortestline2", 2 );

    t.add( "makepolygon1", 3 );
    t.add( "makepolygon2", 3 );
    t.add( "st_makepolygon1", 3 );
    t.add( "st_makepolygon2", 3 );

    t.add( "extractmultipoint1", 4 );
    t.add( "extractmultilinestring1", 5 );
    t.add( "extractmultipolygon1", 6 );

    // geometry functions that do not touch their types
    t.add( "snap3", -1 );
    t.add( "st_snap3", -1 );
    t.add( "makevalid1", -1 );
    t.add( "st_makevalid1", -1 );
    t.add( "offsetcurve3", -1 );
    t.add( "st_offsetcurve3", -1 );
    t.add( "transform2", -1, 1 );
    t.add( "st_transform2", -1, 1 );
    t.add( "setsrid2", -1, 1 );
    t.add( "st_transform2", -1, 1 );
    t.add( "st_translate4", -1 );
    t.add( "st_shift_longitude1", -1 );
    t.add( "normalizelonlat1", -1 );
    t.add( "scalecoords2", -1 );
    t.add( "scalecoords3", -1 );
    t.add( "scalecoordinates2", -1 );
    t.add( "scalecoordinates3", -1 );
    t.add( "rotatecoords2", -1 );
    t.add( "rotatecoordinates2", -1 );
    t.add( "reflectcoords3", -1 );
    t.add( "reflectcoordinates3", -1 );
    t.add( "swapcoords1", -1 );
    t.add( "swapcoordinates1", -1 );

    // special functions

    // geomfromtext
    // geomfromwkb
    // casttomulti
    // castosingle
    // casttoxy
    // casttoxyz
    // casttoxym
    // casttoxyzm
    // intersection
    // difference
    // gunion
    // symdifference
    return t;
}

int intersectionType( int ta, int tb )
{
    if (ta > tb) {
        return intersectionType(tb,ta);
    }
    // ta < tb
    switch (ta)
    {
    case 1:
        return 1;
    case 2:
        switch (tb) {
        case 2:
            return 1;
        case 3:
            return 2;
        case 4:
            return 4;
        case 5:
            return 4;
        case 6:
            return 5;
        }
    case 3:
        switch (tb) {
        case 3:
            return 3;
        case 4:
            return 4;
        case 5:
            return 5;
        case 6:
            return 6;
        }
    case 4:
        switch (tb) {
        case 4:
            return 4;
        case 5:
            return 4;
        case 6:
            return 4;
        }
    case 5:
        switch (tb)
        {
        case 5:
            return 4;
        case 6:
            return 5;
        }
    case 6:
        return 6;
    };
    return 0;
}

int unionType( int ta, int tb )
{
    switch (ta) {
    case 1:
    case 4:
        if (tb == 1 || tb == 4) {
            return 4;
        }
        break;
    case 2:
    case 5:
        if (tb == 2 || tb == 5) {
            return 5;
        }
        break;
    case 3:
    case 6:
        if (tb == 2 || tb == 5) {
            return 6;
        }
        break;
    }
    // else its a collection
    return 7;
}

int differenceType( int ta, int tb )
{
    switch (ta)
    {
    case 1:
    case 4:
        return ta;
    case 2:
    case 5:
        switch (tb) {
        case 2:
        case 5:
        case 3:
        case 6:
            return ta;
        }
    case 3:
    case 6:
        switch (tb) {
        case 3:
        case 6:
            return ta;
        }
    }
    // else invalid
    return 0;
}

class ColumnTypeInferer : public DFSVisitor
{
public:
    ColumnTypeInferer( const TableDefs* defs ) : DFSVisitor(), defs_(defs)
    {
    }

    ~ColumnTypeInferer()
    {
    }

    ColumnDef eval( const Node& n )
    {
        // save current context
        ColumnDef *pColumn = column;
        ColumnDef c;
        column = &c;
        n.accept(*this);
        // restore context
        column = pColumn;
        return c;
    }

    virtual void visit( const Select& s ) override
    {
        // first evaluate FROM
        if ( s.from() ) {
            s.from()->accept( *this );
        }
        // then the columns
        s.column_list()->accept( *this );
    }

    virtual void visit( const TableName& t ) override
    {
        // add all the columns of the table to the ref
        if ( defs_->find( t.name() ) != defs_->end() ) {
            for ( auto& c : (*defs_)[t.name()] ) {
                refs_[t.name()] << c;
            }
        }
    }

    virtual void visit( const TableSelect& s ) override
    {
        // subquery (recursive call)
        QList<ColumnDef> o = columnTypes_r( *s.select(), defs_ );

        if ( refs_.find(s.alias()) == refs_.end() ) {
            refs_[s.alias()] = TableDef();
        }

        for ( auto& c : o ) {
            refs_[s.alias()]  << c;
        }
    }

    virtual void visit( const AllColumns& c ) override
    {
        if ( !c.table().isEmpty() ) {
            if ( refs_.find(c.table()) == refs_.end()) {
                throw InfererException( "Unknown table " + c.table() );
            }
            // add all columns of the given table
            types.append( refs_[c.table()] );
        }
        else {
            // add all columns of ALL the tables
            for ( auto& r : refs_ ) {
                types.append( r );
            }
        }
    }

    virtual void visit( const TableColumn& c ) override
    {
        if ( !c.table().isEmpty() ) {
            // set the current column
            QList<ColumnDef> cdefs = refs_[c.table()].findColumn( c.column() );
            if (cdefs.isEmpty()) {
                throw InfererException("Cannot find column " + c.column() + " in table " + c.table() );
            }
            *column = cdefs[0];
        }
        else {
            // look for the column in ALL tables
            QList<ColumnDef> cdefs = refs_.findColumn( c.column() );
            if (cdefs.isEmpty()) {
                throw InfererException("Cannot find column " + c.column() );
            }
            if (cdefs.size() > 1) {
                throw InfererException("Ambiguous column name " + c.column() );
            }
            *column = cdefs[0];
        }
    }

    virtual void visit( const ColumnExpression& c ) override
    {
        ColumnDef cdef = eval( *c.expression() );

        if ( ! c.alias().isEmpty() ) {
            cdef.setName( c.alias() );
        }
        types << cdef;
    }

    virtual void visit( const ExpressionFunction& c ) override
    {
        QVector<ColumnDef> argsDefs;
        bool allConstants = true;
        for ( auto& arg: *c.args() ) {
            argsDefs << eval( *arg );
            if (!argsDefs.back().isConstant()) {
                allConstants = false;
            }
        }

        //TODO constant evaluation
        column->setConstant( false );
        QString fname = c.name().toLower();
        QString h = c.name().toLower() + QString::number(argsDefs.size());
        OutputFunctionTypes::const_iterator r = outputFunctionTypes.find(h);
        if ( r != outputFunctionTypes.end() ) {
            if ( r->type != QVariant::UserType ) {
                column->setScalarType( r->type );
            }
            else {
                ColumnDef geodef;
                // take the first geometry argument found
                for ( ColumnDef def: argsDefs ) {
                    if ( def.isGeometry() ) {
                        geodef.setGeometry( def.wkbType() );
                        geodef.setSrid( def.srid() );
                        break;
                    }
                }

                if ( r->wkbType == -1 ) {
                    // copy wkbtype from arguments
                    if ( geodef.isGeometry() ) {
                        column->setGeometry( geodef.wkbType() );
                    }
                    else {
                        // undetermined geometry
                        column->setGeometry( 0 );
                    }
                }
                else {
                    column->setGeometry( r->wkbType );
                }
                if ( (r->sridParameter != -1) && argsDefs[r->sridParameter].isConstant() ) {
                    QVariant v = argsDefs[r->sridParameter].value();
                    column->setSrid( v.toInt() );
                }
                else {
                    column->setSrid( geodef.srid() );
                }
            }
        }
        else if ( ((h == "casttomulti1") || (h == "st_multi1")) && argsDefs[0].isGeometry() ) {
            if (argsDefs[0].wkbType() & 7 <= 3) {
                // cast to multi
                column->setGeometry( argsDefs[0].wkbType() + 3 );
            }
            else {
                column->setGeometry( argsDefs[0].wkbType() );
            }
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttosingle1") && argsDefs[0].isGeometry() ) {
            if (argsDefs[0].wkbType() & 7 > 3) {
                // cast to single
                column->setGeometry( argsDefs[0].wkbType() - 3 );
            }
            else {
                column->setGeometry( argsDefs[0].wkbType() );
            }
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxy1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( argsDefs[0].wkbType() & 7 );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxyz1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( 1000 + (argsDefs[0].wkbType() & 7) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxym1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( 2000 + (argsDefs[0].wkbType() & 7) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxyzm1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( 3000 + (argsDefs[0].wkbType() & 7) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( ((h == "intersection2") || (h == "st_intersection2")) && argsDefs[0].isGeometry() && argsDefs[1].isGeometry() ) {
            column->setGeometry( intersectionType( argsDefs[0].wkbType(), argsDefs[1].wkbType() ) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( ((h == "difference2") || (h == "st_difference2") || (h == "symdifference2") || (h == "st_symdifference2"))
                  && argsDefs[0].isGeometry() && argsDefs[1].isGeometry() ) {
            column->setGeometry( differenceType( argsDefs[0].wkbType(), argsDefs[1].wkbType() ) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( ((h == "gunion2") || (h == "st_union2")) && argsDefs[0].isGeometry() && argsDefs[1].isGeometry() ) {
            column->setGeometry( unionType( argsDefs[0].wkbType(), argsDefs[1].wkbType() ) );
            column->setSrid( argsDefs[0].srid() );
        }
        else {
            // can't find function, don't know the final type
            column->setScalarType( QVariant::Invalid );
        }
    }

    virtual void visit( const ExpressionLiteral& l ) override
    {
        column->setConstantValue( l.value() );
    }

    virtual void visit( const ExpressionBinaryOperator& b ) override
    {
        ColumnDef left, right;

        left = eval( *b.left() );
        right = eval( *b.right() );

        if (left.isConstant() && right.isConstant()) {
            // reuse QgsExpression
            QgsExpression::NodeLiteral* nLeft( new QgsExpression::NodeLiteral( left.value() ) );
            QgsExpression::NodeLiteral* nRight( new QgsExpression::NodeLiteral( right.value() ) );
            QScopedPointer<QgsExpression::NodeBinaryOperator> nOp( new QgsExpression::NodeBinaryOperator( b.op(), nLeft, nRight ) );
            QgsFeature f;
            QgsExpression e("");
            QVariant result = nOp->eval( &e, &f );
            column->setConstantValue( result );
        }
        else {
            column->setConstant( false );
            if ( ((b.op() >= QgsExpression::boOr) && (b.op() <= QgsExpression::boIsNot)) || (b.op() == QgsExpression::boMod) ) {
                // boolean and int operations
                column->setScalarType(QVariant::Int);
            }
            else if ( (b.op() >= QgsExpression::boPlus) && (b.op() <= QgsExpression::boDiv) ) {
                column->setScalarType( resultingArithmeticType( left.scalarType(), right.scalarType() ) );
            }
            else {
                // boConcat
                column->setScalarType( QVariant::String );
            }
        }
    }

    virtual void visit( const ExpressionUnaryOperator& b ) override
    {
        ColumnDef left;

        left = eval( *b.expression() );

        if ( left.isConstant() ) {
            // reuse QgsExpression
            QgsExpression::NodeLiteral* nLeft( new QgsExpression::NodeLiteral( left.value() ) );
            QScopedPointer<QgsExpression::NodeUnaryOperator> nOp( new QgsExpression::NodeUnaryOperator( b.op(), nLeft ) );
            QgsFeature f;
            QgsExpression e("");
            QVariant result = nOp->eval( &e, &f );
            column->setConstantValue( result );
        }
        else {
            column->setConstant(false);
            if ( b.op() == QgsExpression::uoNot ) {
                column->setScalarType(QVariant::Int);
            }
            else {
                if ((left.scalarType() == QVariant::Int) || (left.scalarType() == QVariant::Double) ){
                    column->setScalarType(left.scalarType());
                }
                else {
                    column->setScalarType(QVariant::Int);
                }
            }
        }
    }

    virtual void visit( const ExpressionCondition& c ) override
    {
        QList<QPair<ColumnDef::Type, const Node*>> possibleTypes;
        bool allConstants = true;
        for ( auto& n: *c.conditions() ) {
            Q_ASSERT( n->type() == Node::NODE_EXPRESSION_WHEN_THEN );
            ExpressionWhenThen* wt = static_cast<ExpressionWhenThen*>(n.get());

            ColumnDef when = eval( *wt->when() );
            if ( allConstants && when.isConstant() ) {
                if ( when.value().toInt() != 0 ) {
                    ColumnDef then_node = eval( *wt->then_node() );
                    *column = then_node;
                    return;
                }
            }
            else {
                allConstants = false;

                ColumnDef then_node = eval( *wt->then_node() );
                possibleTypes << qMakePair(then_node.type(), wt->then_node());
            }
        }
        ColumnDef elseNode = eval( *c.else_node() );
        if (allConstants) {
            *column = elseNode;
            return;
        }
        else {
            possibleTypes << qMakePair(elseNode.type(), c.else_node() );
        }
        ColumnDef::Type lastType = possibleTypes.front().first;
        QString lastTypeNodeStr = asString( *possibleTypes.front().second );
        for ( auto& t : possibleTypes ) {
            if ( t.first != lastType ) {
                QString nodeStr = asString( *t.second );
                throw InfererException("Type mismatch between " + lastTypeNodeStr + " and " + nodeStr);
            }
        }
        column->setConstant( false );
        column->setType( lastType );
    }

    virtual void visit( const ExpressionIn& c ) override
    {
        // TODO constant evaluation
        column->setConstant( false );
        column->setScalarType( QVariant::Int );
    }

    virtual void visit( const ExpressionSubQuery& t ) override
    {
        ColumnDef select = eval( *t.select() );
        if ( select.isConstant() ) {
            if ( t.exists() ) {
                column->setConstantValue( 1 );
            }
            else {
                column->setConstantValue( select.value() );
            }
        }
        else {
            column->setConstant( false );
            if ( t.exists() ) {
                column->setScalarType( QVariant::Int );
            }
            else {
                column->setScalarType( select.scalarType() );
            }
        }
    }

    virtual void visit( const ExpressionCast& t ) override
    {
        ColumnDef expr = eval( *t.expression() );
        if ( expr.isConstant() ) {
            if ( t.type() == QVariant::Int ) {
                column->setConstantValue( expr.value().toInt() );
            }
            else if ( t.type() == QVariant::Double ) {
                column->setConstantValue( expr.value().toDouble() );
            }
            else {
                column->setConstantValue( expr.value().toString() );
            }
        }
        else {
            column->setConstant( false );
            column->setScalarType( t.type() );
        }
    }

    // resulting column types
    QList<ColumnDef> types;

    // current column
    ColumnDef* column;

    static const OutputFunctionTypes outputFunctionTypes;
private:
    // definitions of tables passed during construction
    const TableDefs* defs_;

    // tables referenced
    TableDefs refs_;

    sqlite3* db_;
};

const OutputFunctionTypes ColumnTypeInferer::outputFunctionTypes = initOutputFunctionTypes();

QList<ColumnDef> columnTypes_r( const Node& n, const TableDefs* tableContext )
{
    ColumnTypeInferer v( tableContext );
    ColumnDef cdef;
    v.column = &cdef;
    n.accept( v );

    return v.types;
}

QList<ColumnDef> columnTypes( const Node& n, QString& errMsg, const TableDefs* tableContext )
{
    QList<ColumnDef> cdefs;
    try
    {
        cdefs = columnTypes_r( n, tableContext );
    }
    catch ( InfererException& e )
    {
        errMsg = e.err();
    }
    return cdefs;
}

} // namespace QgsSql
