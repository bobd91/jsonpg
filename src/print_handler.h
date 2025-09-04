

#define JSONPG_HANDLER_RECEIVE_PREFIX   print_
#define JSONPG_HANDLER_ASSERT_CTX

void print_indent(PrintContext ctx, JsonOutputStream os)
{
        if(ctx->comma)
                jos_put(os, ',')
#ifdef JSONPG_PRETTY_INDENT
        if(ctx->newline)
                jos_put(is, '\n');
        jos_putn(os, ' ', JSONPG_PRETTY_INDENT * ctx->level);
#endif
}

void print_null(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_puts(os, "null", 4);
}

void print_boolean(PrintContext ctx, bool is_true)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        if(is_true)
                jos_puts(os, "true,", 5);
        else
                jos_puts(os, "false,", 6);
}

void print_string_bytes(PrintContext ctx, Bytes bytes, size_t count)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, '"');
        jos_escape(os, bytes, count);
        jos_puts(os, "\",", 2);
}

void print_key_bytes(PrintContext ctx, Bytes bytes, size_t count)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, '"');
        jos_escape(os, bytes, count);
        jos_puts(os, "\":", 2);
}

void print_integer(PrintContext ctx, long integer)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_puti(os, integer);
        jos_put(os, ',');
}

void print_double(PrintContext ctx, double real)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_putr(os, real);
        jos_put(os, ',');
}

void print_start_object(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, '{');
}

void print_end_object(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, '}');
}

void print_start_array(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, '[');
}

void print_end_array(PrintContext ctx)
{
        JsonOutputStream os = ctx->os;
        print_indent(ctx, os);
        jos_put(os, ']');
}












        
        


        




        
        


        













        
        


        




        
        


        

