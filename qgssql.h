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

#include <QSharedPointer>
#include <QScopedPointer>

#include <qgsexpression.h>
#include <qgis.h>

namespace QgsSql
{
    class NodeVisitor;
    class Node
    {
    public:
        enum Type
        {
            NODE_EXPRESSION,
            NODE_EXPRESSION_LITERAL,
            NODE_EXPRESSION_FUNCTION,
            NODE_EXPRESSION_IN,
            NODE_EXPRESSION_BINARY_OP,
            NODE_EXPRESSION_UNARY_OP,
            NODE_EXPRESSION_CONDITION,
            NODE_EXPRESSION_WHEN_THEN,
            NODE_LIST,
            NODE_SELECT,
            NODE_SELECT_STMT,
            NODE_COMPOUND_SELECT,
            NODE_TABLE_COLUMN,
            NODE_EXPRESSION_COLUMN,
            NODE_TABLE_NAME,
            NODE_TABLE_SELECT,
            NODE_JOINED_TABLE,
            NODE_GROUP_BY,
            NODE_HAVING,
            NODE_ORDER_BY,
            NODE_LIMIT_OFFSET,
            NODE_ORDERING_TERM,
            NODE_COLUMN_EXPRESSION,
            NODE_ALL_COLUMNS,
            NODE_EXPRESSION_SUBQUERY,
            NODE_EXPRESSION_CAST
        };
        Node(Type type): mType(type) {}

        virtual void accept( NodeVisitor& v ) const;

        Type type() const { return mType; }
    private:
        Type mType;
    };

    class List : public Node
    {
    public:
        List() : Node(NODE_LIST) {}

        void append( Node* n ) {
            mList.push_back( QSharedPointer<Node>(n) );
        }

        size_t count() const { return mList.size(); }

        // What we would really like here
        // is a list of uniquely owned pointers (std::list<std::unique_ptr>>)
        typedef QList<QSharedPointer<Node>> NodeList;

        NodeList::const_iterator begin() const { return mList.begin(); }
        NodeList::const_iterator end() const { return mList.end(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        NodeList mList;
    };

    class Expression : public Node
    {
    public:
        Expression( Node::Type type ) : Node(type) {}
        void accept( NodeVisitor& v ) const;
    };

    class ExpressionLiteral : public Expression
    {
    public:
        ExpressionLiteral( QVariant value ) :
            Expression( NODE_EXPRESSION_LITERAL ),
            mValue(value)
        {}

        QVariant value() const { return mValue; }

        void accept( NodeVisitor& v ) const;
    private:
        QVariant mValue;
    };

    class ExpressionBinaryOperator : public Expression
    {
    public:
        ExpressionBinaryOperator( QgsExpression::BinaryOperator op, Expression* left, Expression* right ):
            Expression( NODE_EXPRESSION_BINARY_OP ),
            mOp(op),
            mLeft(left),
            mRight(right)
        {}

        const Expression* left() const { return mLeft.data(); }
        const Expression* right() const { return mRight.data(); }

        QgsExpression::BinaryOperator op() const { return mOp; }

        void accept( NodeVisitor& v ) const;
    private:
        QgsExpression::BinaryOperator mOp;
        QScopedPointer<Expression> mLeft;
        QScopedPointer<Expression> mRight;
    };

    class ExpressionUnaryOperator : public Expression
    {
    public:
        ExpressionUnaryOperator( QgsExpression::UnaryOperator op, Expression* expr ):
            Expression( NODE_EXPRESSION_UNARY_OP ),
            mOp(op),
            mExpr(expr)
        {}

        QgsExpression::UnaryOperator op() const { return mOp; }
        const Expression* expression() const { return mExpr.data(); }

        void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Expression> mExpr;
        QgsExpression::UnaryOperator mOp;
    };

    class ExpressionFunction : public Expression
    {
    public:
      //!
      //! @param is_all Whether it's a '*' argument (count(*) for instance)
      //! @param is_distinct Whether the "DISTINCT" keyword is present in front of arguments
      ExpressionFunction( const QString& name, List* args, bool is_all = false, bool is_distinct = false ):
            Expression( NODE_EXPRESSION_FUNCTION ),
            mName( name ),
            mArgs( args ),
            mIsAll( is_all ),
            mIsDistinct( is_distinct )
        {}

        QString name() const { return mName; }
        const List* args() const { return mArgs.data(); }

      bool isAll() const { return mIsAll; }
      bool isDistinct() const { return mIsDistinct; }

        void accept( NodeVisitor& v ) const;
    private:
        QString mName;
        QScopedPointer<List> mArgs;
      bool mIsAll;
      bool mIsDistinct;
    };

    class ExpressionCondition : public Expression
    {
    public:
        ExpressionCondition( List* conditions, Node* else_node = 0 ) :
            Expression( NODE_EXPRESSION_CONDITION ),
            mConditions(conditions),
            mElseNode(else_node)
        {}

        const List* conditions() const { return mConditions.data(); }
        const Node* elseNode() const { return mElseNode.data(); }

        void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<List> mConditions;
        QScopedPointer<Node> mElseNode;
    };

    class ExpressionWhenThen : public Expression
    {
    public:
        ExpressionWhenThen( Node* when, Node* then_node ) :
            Expression( NODE_EXPRESSION_WHEN_THEN ),
            mWhen(when),
            mThen(then_node)
        {}

        const Node* when() const { return mWhen.data(); }
        const Node* thenNode() const { return mThen.data(); }

        void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Node> mWhen, mThen;
    };

    class TableColumn : public Expression
    {
    public:
        TableColumn( const QString& column, const QString& table = "" ) :
            Expression(NODE_TABLE_COLUMN),
            mColumn(column),
            mTable(table) {}

        QString table() const { return mTable; }
        QString column() const { return mColumn; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString mColumn, mTable;
    };

    class TableName : public Node
    {
    public:
        TableName( const QString& name, const QString& alias = "" ) : Node(NODE_TABLE_NAME), mName(name), mAlias(alias) {}

        QString name() const { return mName; }
        QString alias() const { return mAlias; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString mName, mAlias;
    };

    class TableSelect : public Node
    {
    public:
        TableSelect( Node* select, const QString& alias = "" ) :
            Node(NODE_TABLE_SELECT),
            mSelect( select ),
            mAlias(alias) {}

        QString alias() const { return mAlias; }

        const Node* select() const { return mSelect.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString mAlias;
        QScopedPointer<Node> mSelect;
    };

    class ColumnExpression : public Node
    {
    public:
        ColumnExpression( Expression* expr, const QString& alias = "" ) :
            Node(NODE_COLUMN_EXPRESSION),
            mExpr( expr ),
            mAlias(alias) {}

        QString alias() const { return mAlias; }
        const Expression* expression() const { return mExpr.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString mAlias;
        QScopedPointer<Expression> mExpr;
    };

    class AllColumns : public Node
    {
    public:
        AllColumns( const QString& table = "" ) :
            Node(NODE_ALL_COLUMNS),
            mTable(table) {}

        QString table() const { return mTable; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString mTable;
    };


    class ExpressionIn : public Expression
    {
    public:
        ExpressionIn( Expression* expr, Node* in_what, bool not_in = false ) :
            Expression(NODE_EXPRESSION_IN),
            mExpr( expr ),
            mNotIn(not_in),
            mInWhat( in_what )
        {}

        const Expression* expression() const { return mExpr.data(); }
        const Node* inWhat() const { return mInWhat.data(); }
        bool notIn() const { return mNotIn; }

        void accept( NodeVisitor& v ) const;
    private:
        bool mNotIn;
        QScopedPointer<Expression> mExpr;
        QScopedPointer<Node> mInWhat;
    };

    class SelectStmt;
    class ExpressionSubQuery : public Expression
    {
    public:
        ExpressionSubQuery( SelectStmt* select, bool existsSubQuery = false ) :
            Expression(NODE_EXPRESSION_SUBQUERY),
            mSelect( select ),
            mExists( existsSubQuery)
        {}

        const SelectStmt* select() const { return mSelect.data(); }
        bool exists() const { return mExists; }

        void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<SelectStmt> mSelect;
        bool mExists;
    };

    class ExpressionCast : public Expression
    {
    public:
        ExpressionCast( Expression* expr, QString type ) :
            Expression(NODE_EXPRESSION_CAST),
            mExpr( expr )
        {
            QString t = type.toLower();
            if (( t == "integer" ) || ( t == "int" )) {
                mType = QVariant::Int;
            }
            else if ((t == "real") || (t == "double")) {
                mType = QVariant::Double;
            }
            else {
                mType = QVariant::String;
            }
        }

        const Expression* expression() const { return mExpr.data(); }
        QVariant::Type type() const { return mType; }

        void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Expression> mExpr;
        QVariant::Type mType;
    };

    class JoinedTable : public Node
    {
    public:
        enum JoinOperator
        {
            JOIN_LEFT,
            JOIN_INNER,
            JOIN_CROSS
        };
        JoinedTable( JoinOperator join_operator ) :
            Node( NODE_JOINED_TABLE ),
            mJoinOperator(join_operator),
            mIsNatural(false)
        {}
        JoinedTable( JoinOperator join_operator, bool is_natural, Node* right, Expression* on_expr ) :
            Node( NODE_JOINED_TABLE ),
            mJoinOperator(join_operator),
            mIsNatural(is_natural),
            mRight(right),
            mOnExpr(on_expr)
        {}
        JoinedTable( JoinOperator join_operator, bool is_natural, Node* right, List* using_columns ) :
            Node( NODE_JOINED_TABLE ),
            mJoinOperator(join_operator),
            mIsNatural(is_natural),
            mRight(right),
            mUsingColumns(using_columns)
        {}

        JoinOperator joinOperator() const { return mJoinOperator; }
        bool isNatural() const { return mIsNatural; }

        const Node* rightTable() const { return mRight.data(); }
        const Expression* onExpression() const { return mOnExpr.data(); }
        const List* usingColumns() const { return mUsingColumns.data(); }

        void setIsNatural( bool natural ) { mIsNatural = natural; }
        void setRightTable( Node* right ) { mRight.reset(right); }
        void setOnExpression( Expression* expr ) { mOnExpr.reset(expr); }
        void setUsingColumns( List* columns ) { mUsingColumns.reset(columns); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        JoinOperator mJoinOperator;
        bool mIsNatural;
        QScopedPointer<Node> mRight;
        QScopedPointer<Expression> mOnExpr;
        QScopedPointer<List> mUsingColumns;
    };

    class Select : public Node
    {
    public:
        Select( Node* column_list, Node* from, Expression* where, bool is_distinct = false ) :
            Node(NODE_SELECT),
            mColumnList(column_list),
            mFrom(from),
            mWhere(where),
            mIsDistinct(is_distinct)
        {}

        const Node* columnList() const { return mColumnList.data(); }
        const Node* from() const { return mFrom.data(); }
        const Expression* where() const { return mWhere.data(); }

        bool isDistinct() const { return mIsDistinct; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Node> mColumnList, mFrom;
        QScopedPointer<Expression> mWhere;
        bool mIsDistinct;
    };

    class CompoundSelect : public Node
    {
    public:
        enum CompoundOperator
        {
            UNION,
            UNION_ALL,
            INTERSECT,
            EXCEPT
        };
        CompoundSelect( Select* select, CompoundOperator op ) :
            Node(NODE_COMPOUND_SELECT),
            mSelect(select),
            mOp(op)
        {}

        const Select* select() const { return mSelect.data(); }
        CompoundOperator compoundOperator() const { return mOp; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Select> mSelect;
        CompoundOperator mOp;
    };


    class Having : public Node
    {
    public:
        Having( Expression* expr ) :
            Node(NODE_HAVING),
            mExpr(expr)
        {}

        const Expression* expression() const { return mExpr.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Expression> mExpr;
    };

    class GroupBy : public Node
    {
    public:
        GroupBy( List* exp, Having* having = 0 ) : 
            Node(NODE_GROUP_BY),
            mExp(exp)
        {}

        const List* expressions() const { return mExp.data(); }
        const Having* having() const { return mHaving.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<List> mExp;
        QScopedPointer<Having> mHaving;
    };

    class LimitOffset : public Node
    {
    public:
        LimitOffset( Expression* limit, Expression* offset = 0 ) :
            Node(NODE_LIMIT_OFFSET),
            mLimit(limit),
            mOffset(offset)
        {}

        const Expression* limit() const { return mLimit.data(); }
        const Expression* offset() const { return mOffset.data(); }

        void setLimit( Expression* limit ) { mLimit.reset(limit); }
        void setOffset( Expression* offset ) { mOffset.reset(offset); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Expression> mLimit, mOffset;
    };

    class OrderingTerm : public Node
    {
    public:
        OrderingTerm( Expression* expr, bool asc ) :
            Node(NODE_ORDERING_TERM),
            mAsc(asc),
            mExpr(expr)
        {}

        const Expression* expression() const { return mExpr.data(); }
        bool asc() const { return mAsc; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<Expression> mExpr;
        bool mAsc;
    };

    class OrderBy : public Node
    {
    public:
        OrderBy( List* terms ) :
            Node(NODE_ORDER_BY ),
            mTerms(terms)
        {}

        const List* terms() const { return mTerms.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<List> mTerms;
    };

    class SelectStmt : public Node
    {
    public:
        SelectStmt( List* selects, OrderBy* order_by, LimitOffset* limit_offset ):
            Node(NODE_SELECT_STMT),
            mSelects(selects),
            mOrderBy(order_by),
            mLimitOffset(limit_offset)
        {}

        const List* selects() const { return mSelects.data(); }
        const OrderBy* orderBy() const { return mOrderBy.data(); }
        const LimitOffset* limitOffset() const { return mLimitOffset.data(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QScopedPointer<List> mSelects;
        QScopedPointer<OrderBy> mOrderBy;
        QScopedPointer<LimitOffset> mLimitOffset;
    };

    class NodeVisitor
    {
    public:
        virtual void visit( const Node& ) {}
        virtual void visit( const Select& ) {}
        virtual void visit( const SelectStmt& ) {}
        virtual void visit( const CompoundSelect& ) {}
        virtual void visit( const List& ) {}
        virtual void visit( const TableColumn& ) {}
        virtual void visit( const TableName& ) {}
        virtual void visit( const TableSelect& ) {}
        virtual void visit( const JoinedTable& ) {}
        virtual void visit( const ExpressionIn& ) {}
        virtual void visit( const ExpressionUnaryOperator& ) {}
        virtual void visit( const ExpressionBinaryOperator& ) {}
        virtual void visit( const ExpressionFunction& ) {}
        virtual void visit( const ExpressionCondition& ) {}
        virtual void visit( const ExpressionWhenThen& ) {}
        virtual void visit( const ExpressionLiteral& ) {}
        virtual void visit( const OrderBy& ) {}
        virtual void visit( const LimitOffset& ) {}
        virtual void visit( const AllColumns& ) {}
        virtual void visit( const ColumnExpression& ) {}
        virtual void visit( const ExpressionSubQuery& ) {}
        virtual void visit( const ExpressionCast& ) {}
    };

    // Depth-first visitor
    class DFSVisitor : public NodeVisitor
    {
    public:
        DFSVisitor() : NodeVisitor() {}

        virtual void visit( const Select& s ) override;
        virtual void visit( const SelectStmt& s ) override;
        virtual void visit( const OrderBy& s ) override;
        virtual void visit( const LimitOffset& s ) override;
        virtual void visit( const CompoundSelect& s ) override;
        virtual void visit( const List& l ) override;
        virtual void visit( const TableSelect& s ) override;
        virtual void visit( const JoinedTable& jt ) override;
        virtual void visit( const ExpressionUnaryOperator& op ) override;
        virtual void visit( const ExpressionBinaryOperator& op ) override;
        virtual void visit( const ExpressionFunction& f ) override;
        virtual void visit( const ExpressionCondition& c ) override;
        virtual void visit( const ExpressionWhenThen& wt ) override;
        virtual void visit( const ExpressionIn& t ) override;
        virtual void visit( const ColumnExpression& ) override;
        virtual void visit( const ExpressionSubQuery& ) override;
        virtual void visit( const ExpressionCast& ) override;
    };


/**
 * Returns an abstract tree of a SQL query
 * The returned pointer should be freed by the caller (we would like to use a std::unique_ptr here)
 */
QgsSql::Node* parseSql( const QString& sql, QString& parseError, bool formatError = false );

/**
 * Format a parsed SQL tree
 */
QString asString( const QgsSql::Node& );

/**
 * Get a list of referenced tables in the query
 */
QList<QString> referencedTables( const QgsSql::Node& );

/**
 * Type used to define the type of a column
 *
 * It can hold a 'scalar' type (int, double, string) or a geometry type (WKB) and an SRID
 */
class ColumnType
{
public:
    struct Type
    {
        QVariant::Type type;
        QGis::WkbType wkbType;
        long srid;

        Type() : type(QVariant::Invalid), wkbType(QGis::WKBUnknown), srid(-1) {}
        Type( QGis::WkbType aWkbType, long aSrid ) : wkbType(aWkbType), srid(aSrid), type(QVariant::UserType) {}
        Type( QVariant::Type aType ) : wkbType(QGis::WKBUnknown), srid(-1), type(aType) {}

        bool operator==( const Type& other ) const {
            if (type == QVariant::UserType) {
                return other.type == QVariant::UserType && other.wkbType == wkbType && other.srid == srid;
            }
            return other.type == type;
        }
        bool operator!=( const Type& other ) const {
            return !operator==(other);
        }

        QString toString() const {
            if (type == QVariant::Invalid) {
                return "null";
            }
            else if ( type == QVariant::Int ) {
                return "int";
            }
            else if ( type == QVariant::Double ) {
                return "real";
            }
            else if ( type == QVariant::String ) {
                return "string";
            }
            else if ( type == QVariant::UserType ) {
                return QString("geometry(%1,%2)").arg(wkbType).arg(srid);
            }
            return "?";
        }
    };

    ColumnType() : mConstant(false) {}
    ColumnType( QString name, QVariant::Type t ) : mConstant(false), mName(name), mType(t) {}
    ColumnType( QString name, QGis::WkbType wkbType, long srid ) : mConstant(false), mName(name), mType(wkbType, srid) {}

    bool isConstant() const { return mConstant; }
    void setConstant( bool c ) { mConstant = c; }

    void setConstantValue( QVariant v ) { mValue = v; mConstant = true; mType.type = v.type(); }
    QVariant value() const { return mValue; }

    QString name() const { return mName; }
    void setName( QString name ) { mName = name; }

    bool isGeometry() const { return mType.type == QVariant::UserType; }
    void setGeometry( QGis::WkbType wkbType ) { mType.type = QVariant::UserType; mType.wkbType = wkbType; }
    long srid() const { return mType.srid; }
    void setSrid( long srid ) { mType.srid = srid; }

    Type type() const { return mType; }
    void setType( Type type ) { mType = type; }

    void setScalarType( QVariant::Type t ) { mType.type = t; }
    QVariant::Type scalarType() const { return mType.type; }
    QGis::WkbType wkbType() const { return mType.wkbType; }

private:
    QString mName;

    Type mType;
    QVariant mValue;
    bool mConstant;
};

/**
 * Return the list of columns of the given parsed tree
 *
 * \param tableIds list of table names that can appear in FROM clauses
 */
QList<ColumnType> columnTypes( const QgsSql::Node& n, const QList<QString>& tables, QString& err );



class TableDef : public QList<ColumnType>
{
public:
    TableDef() : QList<ColumnType>() {}

    QList<ColumnType> findColumn( QString name ) const;

    QgsFields toFields() const;
};

class TableDefs : public QMap<QString, TableDef>
{
public:
    TableDefs() : QMap<QString, TableDef>() {}

    QList<ColumnType> findColumn( QString name ) const {
        QList<ColumnType> cdefs;
        foreach ( const TableDef& t, *this ) {
            cdefs.append( t.findColumn( name ) );
        }
        return cdefs;
    }


    void addColumnsFromLayer( const QString& tableName, const QgsVectorLayer* vl );
};

/**
 * Return the list of columns of the given parsed tree
 *
 * \param tableDefs list of table definitions
 */
QList<ColumnType> columnTypes( const Node& n, QString& errMsg, const TableDefs* tableContext = 0 );

} // namespace QgsSql
