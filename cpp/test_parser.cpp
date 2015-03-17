#include <QtTest/QtTest>
#include <QObject>
#include <QApplication>
#include <QDesktopServices>

#include <qgsapplication.h>

#include "qgssql.h"

class TestSqlParser : public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.

    void testParsing();
    void testParsing2();
    void testRefTables();
    void testColumnTypes();
};

void TestSqlParser::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
}

void TestSqlParser::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

void TestSqlParser::testParsing()
{
    QString err;
    std::unique_ptr<QgsSql::Node> n( QgsSql::parseSql( "Select * From table", err ) );
    if ( !n ) {
        std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
        return;
    }

    // test error handling
    n = QgsSql::parseSql( "Select * form table", err );
    QVERIFY( !n );
    QVERIFY( err == "1:10: syntax error, unexpected IDENTIFIER, expecting $end" );
}

void TestSqlParser::testParsing2()
{
    QString err;
    std::unique_ptr<QgsSql::Node> n = QgsSql::parseSql( "select *, geometry as geom from departements", err );
    std::cout << err.toUtf8().constData() << std::endl;
    QVERIFY( n != 0 );
}

void TestSqlParser::testRefTables()
{
    QString err;
    std::unique_ptr<QgsSql::Node> n( QgsSql::parseSql( "Select * From table, (select * from table2) as tt WHERE a IN (select id FROM table3)", err ) );
    if ( !n ) {
        std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
        return;
    }

    QList<QString> tables = QgsSql::referencedTables( *n );
    QVERIFY( tables.size() == 3 );

    QVERIFY( tables.contains("table") );
    QVERIFY( tables.contains("table2") );
    QVERIFY( tables.contains("table3") );
}


void TestSqlParser::testColumnTypes()
{
    using namespace QgsSql;

    TableDefs t;
    t["t"] = TableDef();
    t["t"] << ColumnType( "geom", QGis::WKBLineString, 4326 ) << ColumnType( "a", QVariant::Int );

    QString err;
    {
        QString sql( "select CAST(abs(-4) AS real) as ab,t2.*,CASE when a+0 THEN 'ok' ELSE 'no' END,t.* from (Select 2+1, PointFromText('',4325+1) as geom2) t2, t" );
        std::unique_ptr<Node> n( parseSql( sql, err ) );
        if ( !n ) {
            std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
            return;
        }
        QList<ColumnType> cdefs = columnTypes( *n, err, &t );
        QVERIFY( cdefs.size() == 6 );
        QVERIFY( cdefs[0].scalarType() == QVariant::Double );
        QVERIFY( cdefs[0].name() == "ab" );

        QVERIFY( cdefs[1].scalarType() == QVariant::Int );
        QVERIFY( cdefs[1].isConstant() );
        QVERIFY( cdefs[1].value() == 3 );

        QVERIFY( cdefs[2].isGeometry() );
        QVERIFY( cdefs[2].wkbType() == QGis::WKBPoint );
        QVERIFY( cdefs[2].srid() == 4326 );
        QVERIFY( cdefs[2].name() == "geom2" );

        QVERIFY( ! cdefs[3].isConstant() );
        QVERIFY( cdefs[3].scalarType() == QVariant::String );

        QVERIFY( cdefs[4].isGeometry() );
        QVERIFY( cdefs[4].wkbType() == QGis::WKBLineString);
        QVERIFY( cdefs[4].srid() == 4326 );
        QVERIFY( cdefs[4].name() == "geom" );

        QVERIFY( !cdefs[5].isConstant() );
        QVERIFY( cdefs[5].scalarType() == QVariant::Int );
        QVERIFY( cdefs[5].name() == "a" );
    }
    {
        // column name
        QString sql( "SELECT a,b,c FROM t" );
        std::unique_ptr<Node> n( parseSql( sql, err ) );
        if ( !n ) {
            std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
            return;
        }
        QList<ColumnType> cdefs = columnTypes( *n, err, &t );
        QVERIFY( err == "Cannot find column b" );
    }
    {
        // constant evaluation
        QString sql( "SELECT CASE WHEN 1 THEN 'ok' ELSE 34 END, 'ok' || 'no' FROM t" );
        std::unique_ptr<Node> n( parseSql( sql, err ) );
        if ( !n ) {
            std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
            return;
        }
        QList<ColumnType> cdefs = columnTypes( *n, err, &t );
        QVERIFY( cdefs.size() == 2 );
        QVERIFY( cdefs[0].isConstant() );
        QVERIFY( cdefs[0].value() == "ok" );
        QVERIFY( cdefs[1].isConstant() );
        QVERIFY( cdefs[1].value() == "okno" );
    }
    {
        // type inferer: type mismatch
        QString sql( "SELECT CASE WHEN a+0 THEN 'ok' ELSE 34 END FROM t" );
        std::unique_ptr<Node> n( parseSql( sql, err ) );
        if ( !n ) {
            std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
            return;
        }
        QList<ColumnType> cdefs = columnTypes( *n, err, &t );
        QVERIFY( err == "Type mismatch between ok and 34" );
    }
    {
        // type inferer: geometry type
        QString sql( "SELECT CastToXYZ(PointFromText('',2154)), SetSrid(GeomFromText(''),1234) FROM t" );
        std::unique_ptr<Node> n( parseSql( sql, err ) );
        if ( !n ) {
            std::cout << "ERROR: " << err.toUtf8().constData() << std::endl;
            return;
        }
        QList<ColumnType> cdefs = columnTypes( *n, err, &t );
        QVERIFY( cdefs.size() == 2 );
        QVERIFY( cdefs[0].isGeometry() );
        QVERIFY( cdefs[0].wkbType() == QGis::WKBPoint25D );
        QVERIFY( cdefs[0].srid() == 2154 );

        QVERIFY( cdefs[1].isGeometry() );
        QVERIFY( cdefs[1].srid() == 1234 );
    }
}

QTEST_MAIN( TestSqlParser )
#include "test_parser.moc"
