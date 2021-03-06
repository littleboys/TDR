#include "generate/generator_sql.h"
#include "symbols.h"
#include "generator.h"
#include "version.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static error_code_t on_document_begin(generator_t *super, const YYLTYPE *yylloc, const char *file_name)
{
	TLIBC_UNUSED(yylloc);
	generator_open(super, file_name, GENERATOR_SQL_SUFFIX);

	generator_printline(super, 0, "/**");
    generator_printline(super, 0, " * Autogenerated by %s Compiler (%s)", PROJECT_NAME, VERSION);
    generator_printline(super, 0, " *");
    generator_printline(super, 0, " * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING");
    generator_printline(super, 0, " *  @generated");
    generator_printline(super, 0, " */");
	generator_printline(super, 0, "");



	
	generator_printline(super, 0, "");
	generator_printline(super, 0, "");
	return E_TD_NOERROR;
}

static error_code_t on_document_end(generator_t *super, const YYLTYPE *yylloc, const char *file_name)
{
	TLIBC_UNUSED(yylloc);
	TLIBC_UNUSED(file_name);

	generator_printline(super, 0, "");

	generator_close(super);
	return E_TD_NOERROR;
}

static error_code_t on_struct_begin(generator_t *super, const YYLTYPE *yylloc, const char * struct_name)
{
	generator_sql_t *self = TLIBC_CONTAINER_OF(super, generator_sql_t, super);
	TLIBC_UNUSED(yylloc);
	self->struct_begin = TRUE;
	self->first_field = TRUE;
	generator_printline(&self->super, 0, "create table `%s`", struct_name);
	generator_printline(&self->super, 0, "(");

	return E_TD_NOERROR;
}

static error_code_t on_field(generator_t *super, const YYLTYPE *yylloc, const syn_field_t *field)
{
	generator_sql_t *self = TLIBC_CONTAINER_OF(super, generator_sql_t, super);

	const syn_simple_type_t *st = NULL;
	if(!self->first_field)
	{
		generator_printline(&self->super, 0, ",");
	}
	else
	{
		self->first_field = FALSE;
	}
	

	generator_print(super, 1, "`%s` ", field->identifier);

	//由于sql语句不支持根据数据返回不同的列， 所以不支持条件判断与据
	if(field->condition.oper != E_EO_NON)
	{
		scanner_error_halt(yylloc, E_LS_SQL_NOT_SUPPORT_CONDITION);
	}

	//容器类型的列应当另外存一个表
	if(field->type.type != E_SNT_SIMPLE)
	{
		scanner_error_halt(yylloc, E_LS_TYPE_ERROR);
	}

	st = symbols_get_real_type(self->super.symbols, &field->type.st);
	switch(st->st)
	{
	case E_ST_INT8:
		generator_print(super, 1, "tinyint(8) signed");
		break;
	case E_ST_INT16:
		generator_print(super, 1, "smallint(16) signed");
		break;
	case E_ST_INT32:
		generator_print(super, 1, "int(32) signed");
		break;
	case E_ST_INT64:
		generator_print(super, 1, "bigint(64) signed");
		break;
	case E_ST_UINT8:
		generator_print(super, 1, "tinyint(8) unsigned");
		break;
	case E_ST_UINT16:
		generator_print(super, 1, "smallint(16) unsigned");
		break;
	case E_ST_UINT32:
		generator_print(super, 1, "int(32) unsigned");
		break;
	case E_ST_UINT64:
		generator_print(super, 1, "bigint(64) unsigned");
		break;
	case E_ST_CHAR:
		generator_print(super, 1, "char(1)");
		break;
	//注意一下实数的有效数字
	case E_ST_DOUBLE:
		generator_print(super, 1, "double");
		break;
	case E_ST_STRING:
		{
			const symbol_t* rtype = symbols_search(super->symbols, "", st->string_length);
			const syn_value_t* val;
			uint64_t ui64 = 0;
					
			assert(rtype != NULL);
			assert(rtype->type == EN_HST_VALUE);
			val = symbols_get_real_value(super->symbols, &rtype->body.val);
			assert(val != NULL);

			switch(val->type)
			{
			case E_ST_INT8:						
			case E_ST_INT16:
			case E_ST_INT32:
			case E_ST_INT64:
				ui64 = val->val.i64;
				break;
			case E_ST_UINT8:						
			case E_ST_UINT16:
			case E_ST_UINT32:
			case E_ST_UINT64:
				ui64 = val->val.ui64;
				break;
			default:
				assert(0);
			}
					
			if(ui64 <= TLIBC_UINT8_MAX)
			{
				generator_print(super, 1, "char(%"PRIu64")", ui64);
			}
			else if(ui64 <= TLIBC_UINT16_MAX)
			{
				generator_print(super, 1, "text(%"PRIu64")", ui64);
			}
			else
			{
				generator_print(super, 1, "longtext(%"PRIu64")", ui64);
			}
				
			break;
		}
	//union和struct存到单独的一个表中
	//enum等同于int32
	case E_ST_REFER:
		{
			scanner_error_halt(yylloc, E_LS_TYPE_ERROR);
			break;
		}
	}

	return E_TD_NOERROR;
}

static error_code_t on_struct_end(generator_t *super, const YYLTYPE *yylloc, const syn_struct_t *pn_struct)
{
	generator_sql_t *self = TLIBC_CONTAINER_OF(super, generator_sql_t, super);
	TLIBC_UNUSED(yylloc);
	TLIBC_UNUSED(pn_struct);

	generator_printline(&self->super, 0, "");
	generator_printline(&self->super, 0, ");");
	generator_printline(&self->super, 0, "");

	self->struct_begin = FALSE;

	return E_TD_NOERROR;
}

void generator_sql_init(generator_sql_t *self, const symbols_t *symbols)
{
	generator_init(&self->super, symbols);

	self->super.on_document_begin = on_document_begin;
	self->super.on_document_end = on_document_end;
	self->super.on_struct_begin = on_struct_begin;
	self->super.on_field = on_field;
	self->super.on_struct_end = on_struct_end;

	self->struct_begin = FALSE;
}
