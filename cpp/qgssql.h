#include <memory>

#include <qgsexpression.h>

class QgsSql
{
 public:
    struct NodeVisitor;
    class Node
    {
    public:
        enum Type
        {
            NODE_EXPRESSION,
            NODE_LIST,
            NODE_SELECT,
            NODE_TABLE_COLUMN,
            NODE_EXPRESSION_COLUMN,
            NODE_TABLE_NAME,
            NODE_TABLE_SELECT,
            NODE_JOINED_TABLE,
            NODE_EXPRESSION_IN
        };
        Node(Type type): type_(type) {}

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }

        Type type() const { return type_; }
    private:
        Type type_;
    };

    class Expression : public Node
    {
    public:
        Expression( QgsExpression::Node* expr ) :
            Node( NODE_EXPRESSION ),
            expr_( expr )
        {}

        const QgsExpression::Node* expression() const { return expr_.get(); }

        void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        std::unique_ptr<QgsExpression::Node> expr_;
    };

    class TableColumn : public Node
    {
    public:
        TableColumn( const QString& column = "*", const QString& table = "" ) :
            Node(NODE_TABLE_COLUMN),
            column_(column),
            table_(table) {}

        QString table() const { return table_; }
        QString column() const { return column_; }

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        QString column_, table_;
    };

    class TableName : public Node
    {
    public:
        TableName( const QString& name, const QString& alias = "" ) : Node(NODE_TABLE_NAME), name_(name), alias_(alias) {}

        QString name() const { return name_; }
        QString alias() const { return alias_; }

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
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

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        QString alias_;
        std::unique_ptr<Node> select_;
    };

    class ExpressionColumn : public Node
    {
    public:
        ExpressionColumn( Expression* expr, const QString& alias = "" ) :
            Node(NODE_EXPRESSION_COLUMN),
            expr_( expr ),
            alias_(alias) {}

        QString alias() const { return alias_; }
        const Expression* expression() const { return expr_.get(); }

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        QString alias_;
        std::unique_ptr<Expression> expr_;
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

        std::list<std::unique_ptr<Node>>::const_iterator begin() const { return l_.begin(); }
        std::list<std::unique_ptr<Node>>::const_iterator end() const { return l_.end(); }

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        std::list<std::unique_ptr<Node>> l_;
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

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
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

        bool is_distinct() const { return is_distinct_; }

        virtual void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        std::unique_ptr<Node> column_list_, from_;
        std::unique_ptr<Expression> where_;
        bool is_distinct_;
    };

    class ExpressionIn : public Node
    {
    public:
        ExpressionIn( Expression* expr, bool positive_predicate, Node* in_what ) :
            Node(NODE_EXPRESSION_IN),
            expr_( expr ),
            positive_(positive_predicate),
            in_what_( in_what )
        {}

        void accept( NodeVisitor& v ) const {
            v.visit( *this );
        }
    private:
        bool positive_;
        std::unique_ptr<Expression> expr_;
        std::unique_ptr<Node> in_what_;
    };

    class NodeVisitor
    {
    public:
        virtual void visit( const Node& ) {}
        virtual void visit( const Select& ) {}
        virtual void visit( const List& ) {}
        virtual void visit( const TableColumn& ) {}
        virtual void visit( const TableName& ) {}
        virtual void visit( const TableSelect& ) {}
        virtual void visit( const JoinedTable& ) {}
    };

    // Depth-first visitor
    class DFSVisitor : public NodeVisitor
    {
    public:
        DFSVisitor() : NodeVisitor() {}

        virtual void visit( const Select& s ) override
        {
            if ( s.column_list() ) {
                s.column_list()->accept( *this );
            }
            if ( s.from() ) {
                s.from()->accept( *this );
            }
        }
        virtual void visit( const List& l ) override
        {
            for ( auto& n: l ) {
                n->accept(*this);
            }
        }
        virtual void visit( const TableSelect& s ) override
        {
            if ( s.select() ) {
                s.select()->accept(*this);
            }
        }
        virtual void visit( const JoinedTable& jt ) override
        {
            if ( jt.right_table() ) {
                jt.right_table()->accept(*this);
            }
        }
    };

    class PrintVisitor : public DFSVisitor
    {
    public:
        PrintVisitor( std::ostream& ostr ) : ostr_(ostr) {}
        virtual void visit( const Select& s ) override
        {
            ostr_ << "SELECT ";
            s.column_list()->accept( *this );
            ostr_ << " FROM ";
            s.from()->accept( *this );
        }
        virtual void visit( const List& l ) override
        {
            ostr_ << "[";
            bool first = true;
            for ( auto& n: l ) {
                if ( first ) {
                    first = false;
                }
                else {
                    ostr_ << ", ";
                }
                n->accept(*this);
            }
            ostr_ << "]";
        }
        virtual void visit( const TableColumn& t ) override
        {
            ostr_ << t.table().toLocal8Bit().constData() << "." << t.column().toLocal8Bit().constData();
        }
        virtual void visit( const TableName& t ) override
        {
            ostr_ << t.name().toLocal8Bit().constData() << " AS " << t.alias().toLocal8Bit().constData();
        }
    private:
        std::ostream& ostr_;
    };
};

QgsSql::Node* parseSql( const QString& sql, QString& parseError );
