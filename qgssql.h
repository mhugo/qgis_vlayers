#include <memory>

#include <QStack>

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
        Node(Type type): type_(type) {}

        virtual void accept( NodeVisitor& v ) const;

        Type type() const { return type_; }
    private:
        Type type_;
    };

    class List : public Node
    {
    public:
        List() : Node(NODE_LIST) {}

        void append( Node* n ) {
            l_.push_back( std::unique_ptr<Node>(n) );
        }
        void append( std::unique_ptr<Node> n ) {
            l_.push_back( std::move(n) );
        }

        size_t count() const { return l_.size(); }

        std::list<std::unique_ptr<Node>>::const_iterator begin() const { return l_.begin(); }
        std::list<std::unique_ptr<Node>>::const_iterator end() const { return l_.end(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::list<std::unique_ptr<Node>> l_;
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
            value_(value)
        {}

        QVariant value() const { return value_; }

        void accept( NodeVisitor& v ) const;
    private:
        QVariant value_;
    };

    class ExpressionBinaryOperator : public Expression
    {
    public:
        ExpressionBinaryOperator( QgsExpression::BinaryOperator op, Expression* left, Expression* right ):
            Expression( NODE_EXPRESSION_BINARY_OP ),
            op_(op),
            left_(left),
            right_(right)
        {}

        const Expression* left() const { return left_.get(); }
        const Expression* right() const { return right_.get(); }

        QgsExpression::BinaryOperator op() const { return op_; }

        void accept( NodeVisitor& v ) const;
    private:
        QgsExpression::BinaryOperator op_;
        std::unique_ptr<Expression> left_;
        std::unique_ptr<Expression> right_;
    };

    class ExpressionUnaryOperator : public Expression
    {
    public:
        ExpressionUnaryOperator( QgsExpression::UnaryOperator op, Expression* expr ):
            Expression( NODE_EXPRESSION_UNARY_OP ),
            op_(op),
            expr_(expr)
        {}

        QgsExpression::UnaryOperator op() const { return op_; }
        const Expression* expression() const { return expr_.get(); }

        void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Expression> expr_;
        QgsExpression::UnaryOperator op_;
    };

    class ExpressionFunction : public Expression
    {
    public:
        ExpressionFunction( const QString& name, List* args ):
            Expression( NODE_EXPRESSION_FUNCTION ),
            name_( name ),
            args_(args)
        {}

        QString name() const { return name_; }
        const List* args() const { return args_.get(); }

        void accept( NodeVisitor& v ) const;
    private:
        QString name_;
        std::unique_ptr<List> args_;
    };

    class ExpressionCondition : public Expression
    {
    public:
        ExpressionCondition( List* conditions, Node* else_node = 0 ) :
            Expression( NODE_EXPRESSION_CONDITION ),
            conditions_(conditions),
            else_node_(else_node)
        {}

        const List* conditions() const { return conditions_.get(); }
        const Node* else_node() const { return else_node_.get(); }

        void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<List> conditions_;
        std::unique_ptr<Node> else_node_;
    };

    class ExpressionWhenThen : public Expression
    {
    public:
        ExpressionWhenThen( Node* when, Node* then_node ) :
            Expression( NODE_EXPRESSION_WHEN_THEN ),
            when_(when),
            then_(then_node)
        {}

        const Node* when() const { return when_.get(); }
        const Node* then_node() const { return then_.get(); }

        void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Node> when_, then_;
    };

    class TableColumn : public Expression
    {
    public:
        TableColumn( const QString& column, const QString& table = "" ) :
            Expression(NODE_TABLE_COLUMN),
            column_(column),
            table_(table) {}

        QString table() const { return table_; }
        QString column() const { return column_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString column_, table_;
    };

    class TableName : public Node
    {
    public:
        TableName( const QString& name, const QString& alias = "" ) : Node(NODE_TABLE_NAME), name_(name), alias_(alias) {}

        QString name() const { return name_; }
        QString alias() const { return alias_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString name_, alias_;
    };

    class TableSelect : public Node
    {
    public:
        TableSelect( Node* select, const QString& alias = "" ) :
            Node(NODE_TABLE_SELECT),
            select_( select ),
            alias_(alias) {}

        QString alias() const { return alias_; }

        const Node* select() const { return select_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString alias_;
        std::unique_ptr<Node> select_;
    };

    class ColumnExpression : public Node
    {
    public:
        ColumnExpression( Expression* expr, const QString& alias = "" ) :
            Node(NODE_COLUMN_EXPRESSION),
            expr_( expr ),
            alias_(alias) {}

        QString alias() const { return alias_; }
        const Expression* expression() const { return expr_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString alias_;
        std::unique_ptr<Expression> expr_;
    };

    class AllColumns : public Node
    {
    public:
        AllColumns( const QString& table = "" ) :
            Node(NODE_ALL_COLUMNS),
            table_(table) {}

        QString table() const { return table_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        QString table_;
    };


    class ExpressionIn : public Expression
    {
    public:
        ExpressionIn( Expression* expr, Node* in_what, bool not_in = false ) :
            Expression(NODE_EXPRESSION_IN),
            expr_( expr ),
            not_in_(not_in),
            in_what_( in_what )
        {}

        const Expression* expression() const { return expr_.get(); }
        const Node* in_what() const { return in_what_.get(); }
        bool not_in() const { return not_in_; }

        void accept( NodeVisitor& v ) const;
    private:
        bool not_in_;
        std::unique_ptr<Expression> expr_;
        std::unique_ptr<Node> in_what_;
    };

    class SelectStmt;
    class ExpressionSubQuery : public Expression
    {
    public:
        ExpressionSubQuery( SelectStmt* select, bool existsSubQuery = false ) :
            Expression(NODE_EXPRESSION_SUBQUERY),
            select_( select ),
            exists_( existsSubQuery)
        {}

        const SelectStmt* select() const { return select_.get(); }
        bool exists() const { return exists_; }

        void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<SelectStmt> select_;
        bool exists_;
    };

    class ExpressionCast : public Expression
    {
    public:
        ExpressionCast( Expression* expr, QString type ) :
            Expression(NODE_EXPRESSION_CAST),
            expr_( expr )
        {
            QString t = type.toLower();
            if (( t == "integer" ) || ( t == "int" )) {
                type_ = QVariant::Int;
            }
            else if ((t == "real") || (t == "double")) {
                type_ = QVariant::Double;
            }
            else {
                type_ = QVariant::String;
            }
        }

        const Expression* expression() const { return expr_.get(); }
        QVariant::Type type() const { return type_; }

        void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Expression> expr_;
        QVariant::Type type_;
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
            join_operator_(join_operator),
            is_natural_(false)
        {}
        JoinedTable( JoinOperator join_operator, bool is_natural, Node* right, Expression* on_expr ) :
            Node( NODE_JOINED_TABLE ),
            join_operator_(join_operator),
            is_natural_(is_natural),
            right_(right),
            on_expr_(on_expr)
        {}
        JoinedTable( JoinOperator join_operator, bool is_natural, Node* right, List* using_columns ) :
            Node( NODE_JOINED_TABLE ),
            join_operator_(join_operator),
            is_natural_(is_natural),
            right_(right),
            using_columns_(using_columns)
        {}

        JoinOperator join_operator() const { return join_operator_; }
        bool is_natural() const { return is_natural_; }

        const Node* right_table() const { return right_.get(); }
        const Expression* on_expression() const { return on_expr_.get(); }
        const List* using_columns() const { return using_columns_.get(); }

        void set_is_natural( bool natural ) { is_natural_ = natural; }
        void set_right_table( Node* right ) { right_.reset(right); }
        void set_on_expression( Expression* expr ) { on_expr_.reset(expr); }
        void set_using_columns( List* columns ) { using_columns_.reset(columns); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        JoinOperator join_operator_;
        bool is_natural_;
        std::unique_ptr<Node> right_;
        std::unique_ptr<Expression> on_expr_;
        std::unique_ptr<List> using_columns_;
    };

    class Select : public Node
    {
    public:
        Select( Node* column_list, Node* from, Expression* where, bool is_distinct = false ) :
            Node(NODE_SELECT),
            column_list_(column_list),
            from_(from),
            where_(where),
            is_distinct_(is_distinct)
        {}

        const Node* column_list() const { return column_list_.get(); }
        const Node* from() const { return from_.get(); }
        const Expression* where() const { return where_.get(); }

        bool is_distinct() const { return is_distinct_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Node> column_list_, from_;
        std::unique_ptr<Expression> where_;
        bool is_distinct_;
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
            select_(select),
            op_(op)
        {}

        const Select* select() const { return select_.get(); }
        CompoundOperator compound_operator() const { return op_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Select> select_;
        CompoundOperator op_;
    };


    class Having : public Node
    {
    public:
        Having( Expression* expr ) :
            Node(NODE_HAVING),
            expr_(expr)
        {}

        const Expression* expression() const { return expr_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Expression> expr_;
    };

    class GroupBy : public Node
    {
    public:
        GroupBy( List* exp, Having* having = 0 ) : 
            Node(NODE_GROUP_BY),
            exp_(exp)
        {}

        const List* expressions() const { return exp_.get(); }
        const Having* having() const { return having_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<List> exp_;
        std::unique_ptr<Having> having_;
    };

    class LimitOffset : public Node
    {
    public:
        LimitOffset( Expression* limit, Expression* offset = 0 ) :
            Node(NODE_LIMIT_OFFSET),
            limit_(limit),
            offset_(offset)
        {}

        const Expression* limit() const { return limit_.get(); }
        const Expression* offset() const { return offset_.get(); }

        void set_limit( Expression* limit ) { limit_.reset(limit); }
        void set_offset( Expression* offset ) { offset_.reset(offset); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Expression> limit_, offset_;
    };

    class OrderingTerm : public Node
    {
    public:
        OrderingTerm( Expression* expr, bool asc ) :
            Node(NODE_ORDERING_TERM),
            asc_(asc),
            expr_(expr)
        {}

        const Expression* expression() const { return expr_.get(); }
        bool asc() const { return asc_; }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<Expression> expr_;
        bool asc_;
    };

    class OrderBy : public Node
    {
    public:
        OrderBy( List* terms ) :
            Node(NODE_ORDER_BY ),
            terms_(terms)
        {}

        const List* terms() const { return terms_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<List> terms_;
    };

    class SelectStmt : public Node
    {
    public:
        SelectStmt( List* selects, OrderBy* order_by, LimitOffset* limit_offset ):
            Node(NODE_SELECT_STMT),
            selects_(selects),
            order_by_(order_by),
            limit_offset_(limit_offset)
        {}

        const List* selects() const { return selects_.get(); }
        const OrderBy* order_by() const { return order_by_.get(); }
        const LimitOffset* limit_offset() const { return limit_offset_.get(); }

        virtual void accept( NodeVisitor& v ) const;
    private:
        std::unique_ptr<List> selects_;
        std::unique_ptr<OrderBy> order_by_;
        std::unique_ptr<LimitOffset> limit_offset_;
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
 * Return an abstract tree of a SQL query
 */
std::unique_ptr<QgsSql::Node> parseSql( const QString& sql, QString& parseError, bool formatError = false );

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

    ColumnType() : constant_(false) {}
    ColumnType( QString name, QVariant::Type t ) : constant_(false), name_(name), type_(t) {}
    ColumnType( QString name, QGis::WkbType wkbType, long srid ) : constant_(false), name_(name), type_(wkbType, srid) {}

    bool isConstant() const { return constant_; }
    void setConstant( bool c ) { constant_ = c; }

    void setConstantValue( QVariant v ) { value_ = v; constant_ = true; type_.type = v.type(); }
    QVariant value() const { return value_; }

    QString name() const { return name_; }
    void setName( QString name ) { name_ = name; }

    bool isGeometry() const { return type_.type == QVariant::UserType; }
    void setGeometry( QGis::WkbType wkbType ) { type_.type = QVariant::UserType; type_.wkbType = wkbType; }
    long srid() const { return type_.srid; }
    void setSrid( long srid ) { type_.srid = srid; }

    Type type() const { return type_; }
    void setType( Type type ) { type_ = type; }

    void setScalarType( QVariant::Type t ) { type_.type = t; }
    QVariant::Type scalarType() const { return type_.type; }
    QGis::WkbType wkbType() const { return type_.wkbType; }

private:
    QString name_;

    Type type_;
    QVariant value_;
    bool constant_;
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

    QList<ColumnType> findColumn( QString name ) const {
        QList<ColumnType> cdefs;
        foreach ( const ColumnType& c, *this ) {
            if ( c.name().toLower() == name.toLower() ) {
                cdefs << c;
            }
        }
        return cdefs;
    }
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
