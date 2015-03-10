#include <qgssql.h>

#include <QSet>

QStack<const QgsSql::Node*> QgsSql::DFSVisitor::Scope::stack;

void QgsSql::DFSVisitor::visit( const Select& s )
{
    Scope scope(s);
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

void QgsSql::DFSVisitor::visit( const SelectStmt& s )
{
    Scope scope(s);
    if ( s.selects() ) { s.selects()->accept(*this); }
    if ( s.order_by() ) { s.order_by()->accept(*this); }
    if ( s.limit_offset() ) { s.limit_offset()->accept(*this); }
}

void QgsSql::DFSVisitor::visit( const OrderBy& s )
{
    Scope scope(s);
    if ( s.terms() ) { s.terms()->accept(*this); }
}

void QgsSql::DFSVisitor::visit( const LimitOffset& s )
{
    Scope scope(s);
    if ( s.limit() ) { s.limit()->accept(*this); }
    if ( s.offset() ) { s.offset()->accept(*this); }
}

void QgsSql::DFSVisitor::visit( const CompoundSelect& s )
{
    Scope scope(s);
    if ( s.select() ) { s.select()->accept(*this); }
}

void QgsSql::DFSVisitor::visit( const List& l )
{
    Scope scope(l);
    for ( auto& n: l ) {
        n->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit( const TableSelect& s )
{
    Scope scope(s);
    if ( s.select() ) {
        s.select()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit( const JoinedTable& jt )
{
    Scope scope(jt);
    if ( jt.right_table() ) {
        jt.right_table()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionUnaryOperator& op )
{
    Scope scope(op);
    if ( op.expression() ) {
        op.expression()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionBinaryOperator& op )
{
    Scope scope(op);
    if ( op.left() ) {
        op.left()->accept(*this);
    }
    if ( op.right() ) {
        op.right()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionFunction& f )
{
    Scope scope(f);
    if (f.args()) {
        f.args()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionCondition& c )
{
    Scope scope(c);
    if (c.conditions()) {
        c.conditions()->accept(*this);
    }
    if (c.else_node()) {
        c.else_node()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionWhenThen& wt )
{
    Scope scope(wt);
    if (wt.when()) {
        wt.when()->accept(*this);
    }
    if (wt.then_node()) {
        wt.then_node()->accept(*this);
    }
}

void QgsSql::DFSVisitor::visit(const ExpressionIn& t )
{
    Scope scope(t);
    if (t.expression()) { t.expression()->accept(*this); }
    if (t.in_what()) { t.in_what()->accept(*this); }
}

class AsStringVisitor : public QgsSql::DFSVisitor
{
public:
    virtual void visit( const QgsSql::Select& s ) override
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
    virtual void visit( const QgsSql::List& l ) override
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
    virtual void visit( const QgsSql::TableColumn& t ) override
    {
        str_ += t.table() + "." + t.column();
    }
    virtual void visit( const QgsSql::TableName& t ) override
    {
        str_ += t.name() + " AS " + t.alias();
    }
    virtual void visit( const QgsSql::ExpressionLiteral& t ) override
    {
        str_ += t.value().toString();
    }
    virtual void visit( const QgsSql::ExpressionIn& t ) override
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

QString asString( const QgsSql::Node& n )
{
    AsStringVisitor pv;
    n.accept(pv);
    return pv.str_;
}

class TableVisitor : public QgsSql::DFSVisitor
{
public:
    TableVisitor() : QgsSql::DFSVisitor() {}

    // collect table names over the query
    virtual void visit( const QgsSql::TableName& t ) override
    {
        tables.insert( t.name() );
    }

    QSet<QString> tables;
};

QList<QString> referencedTables( const QgsSql::Node& n )
{
    TableVisitor tv;
    n.accept(tv);
    return tv.tables.toList();
}
