#include "Scope.h"
#include "Utils.hpp"
#include <cassert>

namespace OVD
{
	Scope::Scope(const std::string_view& scope_string)
	{
		parse(scope_string, variables, callables, definitions);
	}

	enum Scope::scope_type Scope::get_scope_type(const std::string_view& substring, std::string &scope_name)
	{
		size_t position = substring.find("class");
		if (position != std::string::npos)
		{
			scope_name = substring.substr(position + sizeof("class"));
			return class_scope;
		}

		position = substring.find("namespace");
		if (position != std::string::npos)
		{
			scope_name = substring.substr(position + +sizeof("namespace"));
			return namespace_scope;
		}

		scope_name = "::";
		return unknown_scope;
	}

	void Scope::parse(const std::string_view& scope_string, std::vector<Variable>& variables, std::vector<Callable>& callables, std::vector<Definition>& definitions)
	{
		tokenise(scope_string, "[[", [&](size_t last_position, size_t current_position)
			{
				size_t attribute_end = scope_string.find("]]", current_position);
				size_t expression_end = scope_string.find_first_of("};", attribute_end);
				size_t function_body_begin = std::string::npos;
				if (scope_string[expression_end] == '}')
					function_body_begin = scope_string.find('{', attribute_end);

				std::string_view attributes_view = std::string_view(scope_string.data() + current_position + 2, scope_string.data() + attribute_end);
				std::string_view expression_view = std::string_view(scope_string.data() + attribute_end + 2, scope_string.data() + expression_end);
				std::string_view function_declaration_view = std::string_view(scope_string.data() + attribute_end + 2, scope_string.data() + std::min(function_body_begin, expression_end));

				size_t parameters_begin = function_declaration_view.find('(');
				size_t parameters_end = std::string::npos;
				if (parameters_begin != std::string::npos)
					parameters_end = function_declaration_view.find(')', parameters_begin);

				Attribute attributes = parse_attributes(std::string_view(scope_string.data() + current_position + 2, scope_string.data() + attribute_end));
				check_attribute_errors(attributes, parameters_begin != std::string::npos, function_body_begin != std::string::npos);

				if ((attributes & Attribute::variable) == Attribute::variable)
				{
					std::string_view type_and_name_view = std::string_view(scope_string.data() + attribute_end + 2, scope_string.data() + expression_end);

					std::string_view type, name;
					parse_type_and_name(type_and_name_view, type, name);
					variables.push_back(Variable{ std::string(type), std::string(name) });
					return;
				}
				else
				{
					std::string_view parameters = std::string_view(function_declaration_view.data() + parameters_begin + 1, function_declaration_view.data() + parameters_end);
					std::string_view return_type_and_name_view = std::string_view(scope_string.data() + attribute_end + 2, scope_string.data() + parameters_begin + attribute_end + 2);

					std::string_view return_type, name;
					parse_type_and_name(return_type_and_name_view, return_type, name);

					Callable callable{ .name = std::string(name), .parameters = parse_parameters(parameters), .return_type = std::string(return_type) };
					if ((attributes & Attribute::callable) == Attribute::callable)
					{
						callables.push_back(callable);
					}

					if ((attributes & Attribute::definition) == Attribute::definition)
					{
						std::vector<Definition::Callee> callees{};
						definitions.push_back(Definition(callable));
						//OVDDefinition definition{};
					}
				}
			}, false);
	}

	Attribute Scope::parse_attributes(const std::string_view& attribute_string)
	{
		Attribute result = Attribute::none;

		tokenise(attribute_string, ',', [&](size_t last_position, size_t current_position)
			{
				result = (Attribute)(result | parse_attribute_component(attribute_string.data() + last_position, attribute_string.data() + current_position));
			});

		return result;
	}

	Attribute Scope::parse_attribute_component(const char* begin, const char* end)
	{
		bool end_trimmed = false;
		if (*begin == ',')++begin;
		if (*end == ',')
		{
			--end; end_trimmed = true;
		}
		while (std::isspace(*begin))++begin;
		while (std::isspace(*end))
		{
			--end;  end_trimmed = true;
		}
		if (end_trimmed)
			++end;
		std::string_view attribute_component_string = std::string_view(begin, end);
		Attribute result = Attribute::none;
		if (attribute_component_string == "ovd::callable")
			result = (Attribute)(result | Attribute::callable);
		if (attribute_component_string == "ovd::defined")
			result = (Attribute)(result | Attribute::definition);
		if (attribute_component_string == "ovd::variable")
			result = (Attribute)(result | Attribute::variable);
		return result;
	}

	void Scope::check_attribute_errors(Attribute attributes, bool hasParameters, bool hasFunctionBody)
	{
		if ((attributes & Attribute::variable) == Attribute::variable)
		{
			assert(!hasParameters);
			assert(!hasFunctionBody);
		}

		if ((attributes & Attribute::definition) == Attribute::definition)
		{
			assert(hasParameters);
			assert(!hasFunctionBody);
		}

		if ((attributes & Attribute::callable) == Attribute::callable)
		{
			assert(hasParameters);
		}
	}

	void Scope::parse_type_and_name(const std::string_view& expression, std::string_view& type, std::string_view& name)
	{
		std::string_view trimmed = trim_whitespace(expression);
		size_t type_end = std::string::npos, name_begin = std::string::npos;
		for (size_t i = 0; i < trimmed.size() && name_begin == std::string::npos; ++i)
		{
			char character = trimmed[i];
			if (type_end == std::string::npos)
			{
				if (std::isspace(character) || character == '&' || character == '*')
				{
					type_end = i;
				}
			}
			else
			{
				if (!(std::isspace(character) || character == '&' || character == '*'))
				{
					name_begin = i;
				}
			}
		}

		type = std::string_view(trimmed.data(), trimmed.data() + type_end);
		name = std::string_view(trimmed.data() + name_begin, trimmed.data() + trimmed.size());
	}

	std::vector<Variable> Scope::parse_parameters(const std::string_view& parameter_list)
	{
		std::vector<Variable> results;

		tokenise(parameter_list, ',', [&](size_t last_position, size_t current_position)
			{
				std::string_view parameter_type_and_name = std::string_view(parameter_list.data() + last_position, parameter_list.data() + current_position);
				parameter_type_and_name = trim_whitespace(parameter_type_and_name);
				std::string_view type, name;
				parse_type_and_name(parameter_type_and_name, type, name);
				results.push_back({ std::string(type), std::string(name) });
			});

		return results;
	}

	std::string_view Scope::trim_whitespace(const std::string_view& to_trim)
	{
		size_t begin = 0, end = to_trim.size() - 1;
		while (std::isspace(to_trim[begin]))++begin;
		while (std::isspace(to_trim[end]))--end;
		return std::string_view(to_trim.data() + begin, to_trim.data() + end + 1);
	}
}