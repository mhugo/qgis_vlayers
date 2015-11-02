/***************************************************************************
           qgssql.h Classes for SQL abtract syntax tree
begin                : Jan, 2014
copyright            : (C) 2014 Hugo Mercier, Oslandia
email                : hugo dot mercier at oslandia dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qgssql.h>
#include <qgsfeature.h>
#include <qgsmaplayerregistry.h>
#include <qgsvectorlayer.h>
#include <qgsvectordataprovider.h>

#include <QSet>

#include <qgsconfig.h>

namespace QgsSql
{

QList<ColumnType> TableDef::findColumn( QString name ) const
{
    QList<ColumnType> cdefs;
    QString n = name.toLower();
    if ( (n == "_rowid_") || (n == "rowid") || (n == "oid") ) {
        ColumnType c( n, QVariant::Int );
        cdefs << c;
        return cdefs;
    }
    foreach ( const ColumnType& c, *this ) {
        if ( c.name().toLower() == n ) {
            cdefs << c;
        }
    }
    return cdefs;
}

QgsFields TableDef::toFields() const
{
    QgsFields fields;
    foreach ( const ColumnType& c, *this ) {
        if ( !c.isGeometry() ) {
            fields.append( QgsField( c.name(), c.scalarType() ) );
        }
    }
    return fields;
}

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
    if ( s.columnList() ) {
        s.columnList()->accept( *this );
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
    if ( s.orderBy() ) { s.orderBy()->accept(*this); }
    if ( s.limitOffset() ) { s.limitOffset()->accept(*this); }
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
    for ( auto it = l.begin(); it != l.end(); it++ ) {
        (*it)->accept(*this);
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
    if ( jt.rightTable() ) {
        jt.rightTable()->accept(*this);
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
    if (c.elseNode()) {
        c.elseNode()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionWhenThen& wt )
{
    if (wt.when()) {
        wt.when()->accept(*this);
    }
    if (wt.thenNode()) {
        wt.thenNode()->accept(*this);
    }
}

void DFSVisitor::visit(const ExpressionIn& t )
{
    if (t.expression()) { t.expression()->accept(*this); }
    if (t.inWhat()) { t.inWhat()->accept(*this); }
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
        mStr += "SELECT ";
        s.columnList()->accept( *this );
        if ( s.from() ) {
            mStr += " FROM ";
            s.from()->accept( *this );
        }
        if ( s.where() ) {
            mStr += " WHERE ";
            s.where()->accept( *this );
        }
    }
    virtual void visit( const List& l ) override
    {
        mStr += "[";
        bool first = true;
		for ( auto it = l.begin(); it != l.end(); it++ ) {
            if ( first ) {
                first = false;
            }
            else {
                mStr += ", ";
            }
            (*it)->accept(*this);
        }
        mStr += "]";
    }
    virtual void visit( const TableColumn& t ) override
    {
        mStr += t.table() + "." + t.column();
    }
    virtual void visit( const TableName& t ) override
    {
        mStr += t.name() + " AS " + t.alias();
    }
    virtual void visit( const ExpressionLiteral& t ) override
    {
        mStr += t.value().toString();
    }
    virtual void visit( const ExpressionIn& t ) override
    {
        t.expression()->accept(*this);
        if (t.notIn()) {
            mStr += " NOT ";
        }
        mStr += " IN ";
        t.inWhat()->accept(*this);
    }
    QString mStr;
};

QString asString( const Node& n )
{
    AsStringVisitor pv;
    n.accept(pv);
    return pv.mStr;
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
    InfererException( const QString& err ) : mErr(err) {}
    QString err() const { return mErr; }
private:
    QString mErr;
};

void TableDefs::addColumnsFromLayer( const QString& tableName, const QgsVectorLayer* vl )
{
    if ( !vl || vl->type() != QgsMapLayer::VectorLayer) {
        return;
    }
    const QgsFields& fields = vl->dataProvider()->fields();
    for ( int i = 0; i < fields.count(); i++ ) {
        (*this)[tableName] << ColumnType( fields.at(i).name(), fields.at(i).type() );
    }
    if ( vl->dataProvider()->geometryType() != QGis::WKBNoGeometry ) {
        (*this)[tableName] << ColumnType( "geometry", vl->dataProvider()->geometryType(), vl->crs().postgisSrid() );
    }
}

QList<ColumnType> columnTypes_r( const Node& n, const TableDefs* tableContext );

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
    QGis::WkbType wkbType;
    int sridParameter;
	FunctionResult( QVariant::Type aType, QGis::WkbType aWkbType, int aSrid ) : 
	  type(aType), wkbType(aWkbType), sridParameter(aSrid) {}
};

class OutputFunctionTypes : public QHash<QString, FunctionResult>
{
 public:
    OutputFunctionTypes() : QHash<QString, FunctionResult>() {}
    virtual ~OutputFunctionTypes() {}

    void add( QString k, QVariant::Type t )
    {
        insert( k, FunctionResult(t, QGis::WKBNoGeometry, -1) );
    }
    void add( QString k, QGis::WkbType wkbType, int sridParameter = -1 ) {
        insert( k, FunctionResult(QVariant::UserType, wkbType, sridParameter) );
    }
};

OutputFunctionTypes initOutputFunctionTypes()
{
    OutputFunctionTypes t;
    t.add( "casttointeger1", QVariant::Int );
    t.add( "casttodouble1", QVariant::Double );
    t.add( "casttotext1", QVariant::String );
    t.add( "st_point2", QGis::WKBPoint );
    t.add( "makepoint2", QGis::WKBPoint );
    t.add( "makepoint3", QGis::WKBPoint, 2 );
    t.add( "makepointz3", QGis::WKBPoint25D );
    t.add( "makepointz4", QGis::WKBPoint25D, 3 );
    t.add( "makeline1", QGis::WKBLineString ); // aggregate
    t.add( "makeline2", QGis::WKBLineString );
    t.add( "pointfromtext1", QGis::WKBPoint );
    t.add( "pointfromtext2", QGis::WKBPoint, 1 );
    t.add( "st_pointfromtext1", QGis::WKBPoint );
    t.add( "st_pointfromtext2", QGis::WKBPoint, 1 );

    t.add( "linefromtext1", QGis::WKBLineString );
    t.add( "linefromtext2", QGis::WKBLineString, 1 );
    t.add( "linestringfromtext1", QGis::WKBLineString );
    t.add( "linestringfromtext2", QGis::WKBLineString, 1 );
    t.add( "st_linefromtext1", QGis::WKBLineString );
    t.add( "st_linefromtext2", QGis::WKBLineString, 1 );
    t.add( "st_linestringfromtext1", QGis::WKBLineString );
    t.add( "st_linestringfromtext2", QGis::WKBLineString, 1 );

    t.add( "polyfromtext1", QGis::WKBPolygon );
    t.add( "polyfromtext2", QGis::WKBPolygon, 1 );
    t.add( "polygonfromtext1", QGis::WKBPolygon );
    t.add( "polygonfromtext2", QGis::WKBPolygon, 1 );
    t.add( "st_polyfromtext1", QGis::WKBPolygon );
    t.add( "st_polyfromtext2", QGis::WKBPolygon, 1 );
    t.add( "st_polygonfromtext1", QGis::WKBPolygon );
    t.add( "st_polygonfromtext2", QGis::WKBPolygon, 1 );

    t.add( "mpointfromtext1", QGis::WKBMultiPoint );
    t.add( "mpointfromtext2", QGis::WKBMultiPoint, 1 );
    t.add( "st_mpointfromtext1", QGis::WKBMultiPoint );
    t.add( "st_mpointfromtext2", QGis::WKBMultiPoint, 1 );
    t.add( "multipointfromtext1", QGis::WKBMultiPoint );
    t.add( "multipointfromtext2", QGis::WKBMultiPoint, 1 );
    t.add( "st_multipointfromtext1", QGis::WKBMultiPoint );
    t.add( "st_multipointfromtext2", QGis::WKBMultiPoint, 1 );

    t.add( "mlinefromtext1", QGis::WKBMultiLineString );
    t.add( "mlinefromtext2", QGis::WKBMultiLineString, 1 );
    t.add( "multilinestringfromtext1", QGis::WKBMultiLineString );
    t.add( "multilinestringfromtext2", QGis::WKBMultiLineString, 1 );
    t.add( "st_mlinefromtext1", QGis::WKBMultiLineString );
    t.add( "st_mlinefromtext2", QGis::WKBMultiLineString, 1 );
    t.add( "st_multilinestringfromtext1", QGis::WKBMultiLineString );
    t.add( "st_multilinestringfromtext2", QGis::WKBMultiLineString, 1 );

    t.add( "mpolyfromtext1", QGis::WKBMultiPolygon );
    t.add( "mpolyfromtext2", QGis::WKBMultiPolygon, 1 );
    t.add( "multipolygonfromtext1", QGis::WKBMultiPolygon );
    t.add( "multipolygonfromtext2", QGis::WKBMultiPolygon, 1 );
    t.add( "st_mpolyfromtext1", QGis::WKBMultiPolygon );
    t.add( "st_mpolyfromtext2", QGis::WKBMultiPolygon, 1 );
    t.add( "st_multipolygonfromtext1", QGis::WKBMultiPolygon );
    t.add( "st_multipolygonfromtext2", QGis::WKBMultiPolygon, 1 );

    t.add( "pointfromwkb1", QGis::WKBPoint );
    t.add( "pointfromwkb2", QGis::WKBPoint, 1 );
    t.add( "st_pointfromwkb1", QGis::WKBPoint );
    t.add( "st_pointfromwkb2", QGis::WKBPoint, 1 );

    t.add( "linefromwkb1", QGis::WKBLineString );
    t.add( "linefromwkb2", QGis::WKBLineString, 1 );
    t.add( "linestringfromwkb1", QGis::WKBLineString );
    t.add( "linestringfromwkb2", QGis::WKBLineString, 1 );
    t.add( "st_linefromwkb1", QGis::WKBLineString );
    t.add( "st_linefromwkb2", QGis::WKBLineString, 1 );
    t.add( "st_linestringfromwkb1", QGis::WKBLineString );
    t.add( "st_linestringfromwkb2", QGis::WKBLineString, 1 );

    t.add( "polyfromwkb1", QGis::WKBPolygon );
    t.add( "polyfromwkb2", QGis::WKBPolygon, 1 );
    t.add( "polygonfromwkb1", QGis::WKBPolygon );
    t.add( "polygonfromwkb2", QGis::WKBPolygon, 1 );
    t.add( "st_polyfromwkb1", QGis::WKBPolygon );
    t.add( "st_polyfromwkb2", QGis::WKBPolygon, 1 );
    t.add( "st_polygonfromwkb1", QGis::WKBPolygon );
    t.add( "st_polygonfromwkb2", QGis::WKBPolygon, 1 );

    t.add( "mpointfromwkb1", QGis::WKBMultiPoint );
    t.add( "mpointfromwkb2", QGis::WKBMultiPoint, 1 );
    t.add( "st_mpointfromwkb1", QGis::WKBMultiPoint );
    t.add( "st_mpointfromwkb2", QGis::WKBMultiPoint, 1 );
    t.add( "multipointfromwkb1", QGis::WKBMultiPoint );
    t.add( "multipointfromwkb2", QGis::WKBMultiPoint, 1 );
    t.add( "st_multipointfromwkb1", QGis::WKBMultiPoint );
    t.add( "st_multipointfromwkb2", QGis::WKBMultiPoint, 1 );

    t.add( "mlinefromwkb1", QGis::WKBMultiLineString );
    t.add( "mlinefromwkb2", QGis::WKBMultiLineString, 1 );
    t.add( "multilinestringfromwkb1", QGis::WKBMultiLineString );
    t.add( "multilinestringfromwkb2", QGis::WKBMultiLineString, 1 );
    t.add( "st_mlinefromwkb1", QGis::WKBMultiLineString );
    t.add( "st_mlinefromwkb2", QGis::WKBMultiLineString, 1 );
    t.add( "st_multilinestringfromwkb1", QGis::WKBMultiLineString );
    t.add( "st_multilinestringfromwkb2", QGis::WKBMultiLineString, 1 );

    t.add( "mpolyfromwkb1", QGis::WKBMultiPolygon );
    t.add( "mpolyfromwkb2", QGis::WKBMultiPolygon, 1 );
    t.add( "multipolygonfromwkb1", QGis::WKBMultiPolygon );
    t.add( "multipolygonfromwkb2", QGis::WKBMultiPolygon, 1 );
    t.add( "st_mpolyfromwkb1", QGis::WKBMultiPolygon );
    t.add( "st_mpolyfromwkb2", QGis::WKBMultiPolygon, 1 );
    t.add( "st_multipolygonfromwkb1", QGis::WKBMultiPolygon );
    t.add( "st_multipolygonfromwkb2", QGis::WKBMultiPolygon, 1 );

    t.add( "envelope1", QGis::WKBPolygon );
    t.add( "st_envelope1", QGis::WKBPolygon );
    t.add( "reverse1", QGis::WKBLineString );
    t.add( "st_reverse1", QGis::WKBLineString );
    t.add( "forcelhr", QGis::WKBPolygon );
    t.add( "st_forcelhr", QGis::WKBPolygon );

    t.add( "casttopoint1", QGis::WKBPoint );
    t.add( "casttolinestring1", QGis::WKBLineString );
    t.add( "casttopolygon1", QGis::WKBPolygon );
    t.add( "casttomultipoint1", QGis::WKBMultiPoint );
    t.add( "casttomultilinestring1", QGis::WKBMultiLineString );
    t.add( "casttomultipolygon1", QGis::WKBMultiPolygon );

    t.add( "startpoint1", QGis::WKBPoint );
    t.add( "st_startpoint1", QGis::WKBPoint );
    t.add( "endpoint1", QGis::WKBPoint );
    t.add( "st_endpoint1", QGis::WKBPoint );
    t.add( "pointonsurface1", QGis::WKBPoint );
    t.add( "st_pointonsurface1", QGis::WKBPoint );
    t.add( "pointn1", QGis::WKBPoint );
    t.add( "st_pointn1", QGis::WKBPoint );
    t.add( "addpoint2", QGis::WKBLineString );
    t.add( "addpoint3", QGis::WKBLineString );
    t.add( "st_addpoint2", QGis::WKBLineString );
    t.add( "st_addpoint3", QGis::WKBLineString );
    t.add( "setpoint3", QGis::WKBLineString );
    t.add( "st_setpoint3", QGis::WKBLineString );
    t.add( "removepoint2", QGis::WKBLineString );
    t.add( "st_removepoint2", QGis::WKBLineString );

    t.add( "centroid1", QGis::WKBPoint );
    t.add( "st_centroid1", QGis::WKBPoint );

    t.add( "exteriorring1", QGis::WKBLineString );
    t.add( "st_exteriorring1", QGis::WKBLineString );
    t.add( "interiorringn2", QGis::WKBLineString );
    t.add( "st_interiorringn2", QGis::WKBLineString );

    t.add( "buffer2", QGis::WKBPolygon );
    t.add( "st_buffer2", QGis::WKBPolygon );
    t.add( "convexhull1", QGis::WKBPolygon );
    t.add( "st_convexhull1", QGis::WKBPolygon );
    t.add( "line_interpolate_point2", QGis::WKBPoint );
    t.add( "st_line_interpolate_point2", QGis::WKBPoint );
    t.add( "line_interpolate_equidistant_points2", QGis::WKBMultiPoint );
    t.add( "st_line_interpolate_equidistant_points2", QGis::WKBMultiPoint );
    t.add( "line_substring3", QGis::WKBLineString );
    t.add( "st_line_substring3", QGis::WKBLineString );
    t.add( "closestpoint2", QGis::WKBPoint );
    t.add( "st_closestpoint2", QGis::WKBPoint );
    t.add( "shortestline2", QGis::WKBLineString );
    t.add( "st_shortestline2", QGis::WKBLineString );

    t.add( "makepolygon1", QGis::WKBPolygon );
    t.add( "makepolygon2", QGis::WKBPolygon );
    t.add( "st_makepolygon1", QGis::WKBPolygon );
    t.add( "st_makepolygon2", QGis::WKBPolygon );

    t.add( "extractmultipoint1", QGis::WKBMultiPoint );
    t.add( "extractmultilinestring1", QGis::WKBMultiLineString );
    t.add( "extractmultipolygon1", QGis::WKBMultiPolygon );

    t.add( "extent1", QGis::WKBPolygon );
    t.add( "st_extent1", QGis::WKBPolygon );
    t.add( "buildarea1", QGis::WKBPolygon );
    t.add( "st_buildarea1", QGis::WKBPolygon );
    t.add( "polygonize1", QGis::WKBPolygon );
    t.add( "st_polygonize1", QGis::WKBPolygon );

    // geometry functions that do not touch their types
    t.add( "snap3", QGis::WKBUnknown );
    t.add( "st_snap3", QGis::WKBUnknown );
    t.add( "makevalid1", QGis::WKBUnknown );
    t.add( "st_makevalid1", QGis::WKBUnknown );
    t.add( "offsetcurve3", QGis::WKBUnknown );
    t.add( "st_offsetcurve3", QGis::WKBUnknown );
    t.add( "transform2", QGis::WKBUnknown, 1 );
    t.add( "st_transform2", QGis::WKBUnknown, 1 );
    t.add( "setsrid2", QGis::WKBUnknown, 1 );
    t.add( "st_transform2", QGis::WKBUnknown, 1 );
    t.add( "st_translate4", QGis::WKBUnknown );
    t.add( "st_shift_longitude1", QGis::WKBUnknown );
    t.add( "normalizelonlat1", QGis::WKBUnknown );
    t.add( "scalecoords2", QGis::WKBUnknown );
    t.add( "scalecoords3", QGis::WKBUnknown );
    t.add( "scalecoordinates2", QGis::WKBUnknown );
    t.add( "scalecoordinates3", QGis::WKBUnknown );
    t.add( "rotatecoords2", QGis::WKBUnknown );
    t.add( "rotatecoordinates2", QGis::WKBUnknown );
    t.add( "reflectcoords3", QGis::WKBUnknown );
    t.add( "reflectcoordinates3", QGis::WKBUnknown );
    t.add( "swapcoords1", QGis::WKBUnknown );
    t.add( "swapcoordinates1", QGis::WKBUnknown );

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

//
// most probable type, since we cannot deduce the type without evaluating
QGis::WkbType intersectionType( QGis::WkbType ta, QGis::WkbType tb )
{
    if (ta > tb) {
        return intersectionType(tb,ta);
    }
    // ta < tb
    switch (ta)
    {
    case QGis::WKBPoint:
        return QGis::WKBPoint;
    case QGis::WKBLineString:
        switch (tb) {
        case QGis::WKBLineString:
            return QGis::WKBPoint;
        case QGis::WKBPolygon:
            return QGis::WKBLineString;
        case QGis::WKBMultiPoint:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiLineString:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiPolygon:
            return QGis::WKBMultiLineString;
        }
    case QGis::WKBPolygon:
        switch (tb) {
        case QGis::WKBPolygon:
            return QGis::WKBPolygon;
        case QGis::WKBMultiPoint:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiLineString:
            return QGis::WKBMultiLineString;
        case QGis::WKBMultiPolygon:
            return QGis::WKBMultiPolygon;
        }
    case QGis::WKBMultiPoint:
        switch (tb) {
        case QGis::WKBMultiPoint:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiLineString:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiPolygon:
            return QGis::WKBMultiPoint;
        }
    case QGis::WKBMultiLineString:
        switch (tb)
        {
        case QGis::WKBMultiLineString:
            return QGis::WKBMultiPoint;
        case QGis::WKBMultiPolygon:
            return QGis::WKBMultiLineString;
        }
    case QGis::WKBMultiPolygon:
        return QGis::WKBMultiPolygon;
    };
    return QGis::WKBUnknown;
}

//
// most probable type, since we cannot deduce the type without evaluating
QGis::WkbType unionType( QGis::WkbType ta, QGis::WkbType tb )
{
    switch (ta) {
    case QGis::WKBPoint:
    case QGis::WKBMultiPoint:
        if (tb == QGis::WKBPoint || tb == QGis::WKBMultiPoint) {
            return QGis::WKBMultiPoint;
        }
        break;
    case QGis::WKBLineString:
    case QGis::WKBMultiLineString:
        if (tb == QGis::WKBLineString || tb == QGis::WKBMultiLineString) {
            return QGis::WKBMultiLineString;
        }
        break;
    case QGis::WKBPolygon:
    case QGis::WKBMultiPolygon:
        if (tb == QGis::WKBLineString || tb == QGis::WKBMultiLineString) {
            return QGis::WKBMultiPolygon;
        }
        break;
    }
    // else its a collection
    return QGis::WKBUnknown;
}

//
// most probable type, since we cannot deduce the type without evaluating
QGis::WkbType differenceType( QGis::WkbType ta, QGis::WkbType tb )
{
    switch (ta)
    {
    case QGis::WKBPoint:
    case QGis::WKBMultiPoint:
        return ta;
    case QGis::WKBLineString:
    case QGis::WKBMultiLineString:
        switch (tb) {
        case QGis::WKBLineString:
        case QGis::WKBMultiLineString:
        case QGis::WKBPolygon:
        case QGis::WKBMultiPolygon:
            return ta;
        }
    case QGis::WKBPolygon:
    case QGis::WKBMultiPolygon:
        switch (tb) {
        case QGis::WKBPolygon:
        case QGis::WKBMultiPolygon:
            return ta;
        }
    }
    // else invalid
    return QGis::WKBUnknown;
}

QGis::WkbType toXYZ( QGis::WkbType ta )
{
    switch (ta) {
    case QGis::WKBPoint:
        return QGis::WKBPoint25D;
    case QGis::WKBMultiPoint:
        return QGis::WKBMultiPoint25D;
    case QGis::WKBLineString:
        return QGis::WKBLineString25D;
    case QGis::WKBMultiLineString:
        return QGis::WKBMultiLineString25D;
    case QGis::WKBPolygon:
        return QGis::WKBPolygon25D;
    case QGis::WKBMultiPolygon:
        return QGis::WKBMultiPolygon25D;
    }
    return ta;
}

class ColumnTypeInferer : public DFSVisitor
{
public:
    ColumnTypeInferer( const TableDefs* defs ) : DFSVisitor(), mDefs(defs)
    {
    }

    ~ColumnTypeInferer()
    {
    }

    ColumnType eval( const Node& n )
    {
        // save current context
        ColumnType *pColumn = column;
        ColumnType c;
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
        s.columnList()->accept( *this );
    }

    virtual void visit( const TableName& t ) override
    {
        // add all the columns of the table to the ref
        if ( mDefs->find( t.name() ) != mDefs->end() ) {
            const auto& m = (*mDefs)[t.name()];
            for ( auto it = m.begin(); it != m.end(); it++ ) {
                mRefs[t.name()] << *it;
                // also add to alias, if any
                if (!t.alias().isEmpty()) {
                    mRefs[t.alias()] << *it;
                }
            }
        }
    }

    virtual void visit( const TableSelect& s ) override
    {
        // subquery (recursive call)
        QList<ColumnType> o = columnTypes_r( *s.select(), mDefs );

        if ( mRefs.find(s.alias()) == mRefs.end() ) {
            mRefs[s.alias()] = TableDef();
        }

        for ( auto it = o.begin(); it != o.end(); it++ ) {
            mRefs[s.alias()]  << *it;
        }
    }

    virtual void visit( const AllColumns& c ) override
    {
        if ( !c.table().isEmpty() ) {
            if ( mRefs.find(c.table()) == mRefs.end()) {
                throw InfererException( "Unknown table " + c.table() );
            }
            // add all columns of the given table
            types.append( mRefs[c.table()] );
        }
        else {
            // add all columns of ALL the tables
            for ( auto it = mRefs.begin(); it != mRefs.end(); it++ ) {
                types.append( *it );
            }
        }
    }

    virtual void visit( const TableColumn& c ) override
    {
        if ( !c.table().isEmpty() ) {
            // set the current column
            QList<ColumnType> cdefs = mRefs[c.table()].findColumn( c.column() );
            if (cdefs.isEmpty()) {
                throw InfererException("Cannot find column " + c.column() + " in table " + c.table() );
            }
            *column = cdefs[0];
        }
        else {
            // look for the column in ALL tables
            QList<ColumnType> cdefs = mRefs.findColumn( c.column() );
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
        ColumnType cdef = eval( *c.expression() );

        if ( ! c.alias().isEmpty() ) {
            cdef.setName( c.alias() );
        }
        types << cdef;
    }

    virtual void visit( const ExpressionFunction& c ) override
    {
        QVector<ColumnType> argsDefs;
        bool allConstants = true;
		const auto& l = *c.args();
        for ( auto it = l.begin(); it != l.end(); it++ ) {
            argsDefs << eval( *(*it) );
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
                ColumnType geodef;
                // take the first geometry argument found
                foreach ( ColumnType def, argsDefs ) {
                    if ( def.isGeometry() ) {
                        geodef.setGeometry( def.wkbType() );
                        geodef.setSrid( def.srid() );
                        break;
                    }
                }

                if ( r->wkbType == QGis::WKBUnknown ) {
                    // copy wkbtype from arguments
                    if ( geodef.isGeometry() ) {
                        column->setGeometry( geodef.wkbType() );
                    }
                    else {
                        // undetermined geometry
                        column->setGeometry( QGis::WKBUnknown );
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
            column->setGeometry( QGis::multiType(argsDefs[0].wkbType()) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttosingle1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( QGis::singleType(argsDefs[0].wkbType()) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxy1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( QGis::flatType(argsDefs[0].wkbType()) );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( (h == "casttoxyz1") && argsDefs[0].isGeometry() ) {
            column->setGeometry( toXYZ(argsDefs[0].wkbType()) );
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
        else if ( ((h == "gunion1") || (h == "st_union1")) && argsDefs[0].isGeometry() ) {
            column->setGeometry( argsDefs[0].wkbType() );
            column->setSrid( argsDefs[0].srid() );
        }
        else if ( ((h == "collect1") || (h == "st_collect1")) && argsDefs[0].isGeometry() ) {
            column->setGeometry( QGis::WKBUnknown );
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
        ColumnType left, right;

        left = eval( *b.left() );
        right = eval( *b.right() );

        if (left.isConstant() && right.isConstant()) {
            // reuse QgsExpression
            QgsExpression::NodeLiteral* nLeft( new QgsExpression::NodeLiteral( left.value() ) );
            QgsExpression::NodeLiteral* nRight( new QgsExpression::NodeLiteral( right.value() ) );
            QScopedPointer<QgsExpression::NodeBinaryOperator> nOp( new QgsExpression::NodeBinaryOperator( b.op(), nLeft, nRight ) );
            QgsFeature f;
            QgsExpression e("");
#if VERSION_INT >= 21100
            QVariant result = nOp->eval( &e, (const QgsExpressionContext*)0 );
#else
            QVariant result = nOp->eval( &e, &f );
#endif
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
        ColumnType left;

        left = eval( *b.expression() );

        if ( left.isConstant() ) {
            // reuse QgsExpression
            QgsExpression::NodeLiteral* nLeft( new QgsExpression::NodeLiteral( left.value() ) );
            QScopedPointer<QgsExpression::NodeUnaryOperator> nOp( new QgsExpression::NodeUnaryOperator( b.op(), nLeft ) );
            QgsFeature f;
            QgsExpression e("");
#if VERSION_INT >= 21100
            QVariant result = nOp->eval( &e, (const QgsExpressionContext*)0 );
#else
            QVariant result = nOp->eval( &e, &f );
#endif
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
        QList<QPair<ColumnType::Type, const Node*>> possibleTypes;
        bool allConstants = true;
		const auto& cc = *c.conditions();
        for ( auto it = cc.begin(); it != cc.end(); it++ ) {
            Q_ASSERT( (*it)->type() == Node::NODE_EXPRESSION_WHEN_THEN );
            ExpressionWhenThen* wt = static_cast<ExpressionWhenThen*>(it->data());

            ColumnType when = eval( *wt->when() );
            if ( allConstants && when.isConstant() ) {
                if ( when.value().toInt() != 0 ) {
                    ColumnType then_node = eval( *wt->thenNode() );
                    *column = then_node;
                    return;
                }
            }
            else {
                allConstants = false;

                ColumnType then_node = eval( *wt->thenNode() );
                possibleTypes << qMakePair(then_node.type(), wt->thenNode());
            }
        }
        ColumnType elseNode = eval( *c.elseNode() );
        if (allConstants) {
            *column = elseNode;
            return;
        }
        else {
            possibleTypes << qMakePair(elseNode.type(), c.elseNode() );
        }
        ColumnType::Type lastType = possibleTypes.front().first;
        QString lastTypeNodeStr = asString( *possibleTypes.front().second );
        for( auto it = possibleTypes.begin(); it != possibleTypes.end(); it++ ) {
            if ( it->first != lastType ) {
                QString nodeStr = asString( *it->second );
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
        ColumnType select = eval( *t.select() );
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
        ColumnType expr = eval( *t.expression() );
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
    QList<ColumnType> types;

    // current column
    ColumnType* column;

    static const OutputFunctionTypes outputFunctionTypes;
private:
    // definitions of tables passed during construction
    const TableDefs* mDefs;

    // tables referenced
    TableDefs mRefs;

    sqlite3* mDb;
};

const OutputFunctionTypes ColumnTypeInferer::outputFunctionTypes = initOutputFunctionTypes();

QList<ColumnType> columnTypes_r( const Node& n, const TableDefs* tableContext )
{
    ColumnTypeInferer v( tableContext );
    ColumnType cdef;
    v.column = &cdef;
    n.accept( v );

    return v.types;
}

QList<ColumnType> columnTypes( const Node& n, QString& errMsg, const TableDefs* tableContext )
{
    QList<ColumnType> cdefs;
    errMsg = "";
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

QList<ColumnType> columnTypes( const Node& n, const QList<QString>& tables, QString& err )
{
    TableDefs tableDefs;
    foreach( const QString& lname, tables ) {
        QList<QgsMapLayer*> l = QgsMapLayerRegistry::instance()->mapLayersByName( lname );
        if ( l.size() == 0 )
            continue;
        if ( l.back()->type() != QgsMapLayer::VectorLayer )
            continue;
        auto vl = static_cast<QgsVectorLayer*>(l.back());
        tableDefs[lname] = TableDef();
        const QgsFields& fields = vl->dataProvider()->fields();
        for ( int i = 0; i < fields.count(); i++ ) {
            tableDefs[lname] << ColumnType( fields.at(i).name(), fields.at(i).type() );
        }
        if ( vl->dataProvider()->geometryType() != QGis::WKBNoGeometry) {
            tableDefs[lname] << ColumnType( "geometry", vl->dataProvider()->geometryType(), vl->crs().postgisSrid() );
        }
    }
    return columnTypes( n, err, &tableDefs );
}

} // namespace QgsSql
