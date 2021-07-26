#pragma once
#include <string_view>
#include <vector>

namespace ppparse
{
	class source_file;

	enum class node_type
	{
		symbol,
		comment,
		keyword,
		operator_token,
		preprocessor_directive,
		attribute,
		function_parameters,
		template_parameters,
		scope,
		expression,
		file,
		unknown
	};

	class graph_node
	{
		friend source_file;

	public:
		graph_node(graph_node * const parent, const std::string_view& node_body, node_type type);
	private:
		source_file* get_owner();

		std::string_view node_body;

		graph_node * const parent;
		std::vector<graph_node *> children;

	protected:
		node_type type;

	protected:
		//parsing functions
		void parse_block();
		void parse_expression(const std::string_view &to_parse);
		size_t parse_section(size_t& position, const std::string_view& source, const std::string_view &open_token, const std::string_view &close_token);
		void comments_and_preprocessor_directives(size_t& position, const std::string_view &source);
		void attributes(size_t& position, const std::string_view& source);
		void template_parameters(size_t& position, const std::string_view& source);
		void keywords(size_t& position, const std::string_view& source);
		void function_parameters(size_t& position, const std::string_view& source);
		void consume_symbol(size_t& position, const std::string_view& source);
		void top_level_scope(size_t& position, const std::string_view& source);
		void parse_scope_or_expression(size_t& position, const std::string_view& source);
		void skip_whitespace(size_t& position, const std::string_view& source);
		bool is_end(size_t position, const std::string_view& source);
	};
}