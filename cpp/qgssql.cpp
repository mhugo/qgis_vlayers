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
    t.add( "abs(i)", QVariant::Int );
    t.add( "abs(r)", QVariant::Double );
    t.add( "abs(s)", QVariant::Double );
    t.add( "abs(g)", QVariant::Double );
    t.add( "pointfromtext(s)", 1 );
    t.add( "pointfromtext(s,i)", 1, 1 );
    t.add( "makepoint(s,s)", 1 );
    t.add( "makepoint(s,s,i)", 1, 2 );
    t.add( "linestringfromtext(s)", 2 );
    t.add( "linestringfromtext(s,i)", 2, 1 );
    t.add( "polygonfromtext(s)", 3 );
    t.add( "polygonfromtext(s,i)", 3 );
    t.add( "transform(g,i)", -1, 1 );
    t.add( "setsrid(g,i)", -1, 1 );
    return t;
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
            cdef.name() = c.alias();
        }
        types << cdef;
    }

    virtual void visit( const ExpressionFunction& c ) override
    {
        QVector<ColumnDef> argsDefs;
        bool allConstants = false;
        for ( auto& arg: *c.args() ) {
            argsDefs << eval( *arg );
            if (!argsDefs.back().isConstant()) {
                allConstants = false;
            }
        }

        if ( allConstants ) {
            // call sqlite for evaluation
            //TODO
        }
        else {
            QString fname = c.name().toLower();
            QString h = c.name().toLower() + "(";
            bool first = true;
            for ( auto& d: argsDefs ) {
                if (!first) {
                    h += ",";
                }
                first = false;
                switch (d.scalarType()) {
                case QVariant::Int:
                    h += "i";
                    break;
                case QVariant::Double:
                    h += "r";
                    break;
                case QVariant::String:
                    h += "s";
                    break;
                case QVariant::UserType:
                    h += "g";
                    break;
                }
            }
            h+=")";
            std::cout << "hash:" << h.toUtf8().constData() << std::endl;
            OutputFunctionTypes::const_iterator r = outputFunctionTypes.find(h);
            if ( r != outputFunctionTypes.end() ) {
                if ( r->type != QVariant::UserType ) {
                    column->setScalarType( r->type );
                }
                else {
                    column->setGeometry( r->wkbType );
                    if ( (r->sridParameter != -1) && argsDefs[r->sridParameter].isConstant() ) {
                        QVariant v = argsDefs[r->sridParameter].value();
                        column->setSrid( v.toInt() );
                    }
                }
            }
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
