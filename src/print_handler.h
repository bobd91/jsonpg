

E#define JSONPG_HANDLER_RECEIVE_PREFIX   print_
#define JSONPG_HANDLER_ASSERT_CTX

bool print_indent(PrintContext ctx, JsonOutputStream os)
{
        if (ctx->comma)
                if(!jos_put(os, ','))
                        return false;

#ifdef JSONPG_PRETTY_INDENT
        if(ctx->newline) {
                if(!jos_put(os, '\n'))
                        return false;
                if(!jos_putn(os, ' ', JSONPG_PRETTY_INDENT * ctx->level))
                        return false;
        } else {
                return jos_put(os, ' ');
        }
#endif
        return true;
}

bool print_null(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_puts(os, "null", 4);
}

bool print_boolean(PrintContext ctx, bool is_true)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && is_true
                        ? jos_puts(os, "true,", 5)
                        : jos_puts(os, "false,", 6);
}

bool print_string_bytes(PrintContext ctx, Bytes bytes, size_t count)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, '"')
                && jos_escape(os, bytes, count)
                && jos_puts(os, "\"", 2);
}

bool print_key_bytes(PrintContext ctx, Bytes bytes, size_t count)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, '"')
                && jos_escape(os, bytes, count)
                && jos_puts(os, "\":", 2);
}

bool print_integer(PrintContext ctx, long integer)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_puti(os, integer)
                && jos_put(os, ',');
}

bool print_real(PrintContext ctx, double real)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_putr(os, real)
                && jos_put(os, ',');
}

bool void print_start_object(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, '{');
}

bool print_end_object(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, '}');
}

bool print_start_array(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, '[');
}

bool print_end_array(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        return print_indent(ctx, os)
                && jos_put(os, ']');
}












        
        


        




        
        


        













        
        


        




        
        


        

