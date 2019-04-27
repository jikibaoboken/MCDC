
// Classes and functions to deal with a Abstract Syntax Tree (AST) for boolean expressions
// 
// The input to this software is a boolean expression given as string. The boolean expression consists
// of terminals/variables/conditions a-z and, in the negated form A-Z. Meaning A ist the same as (NOT a).
// As operators are defined: AND, OR, XOR, NOT, BRACKETS. Details are shown in the grammar file.
//
// A compiler, sonsisting of a Scanner, Parser and Code Generator, reads the source boolean expression
// and converts it to a Abstract Syntax Tree. The Abstract syntax tree is implemented as a vector of
// tree elements with a linkage to children and parents. The linkage is done via the index in the vector.
// If a tree element has no parent, then it is the root element. A binary tree, meaning a alement (node)
// can have maximum 2 children. This approach is used, because boolean operations are binary as well.
// So, each node can have 0, 1 or 2 children. In case of 0 children, the node is a leaf, and denotes 
// a variable. 2 children are for Standard binary boolean operations. And for example, the unary
// not has only one child.
//
// The AST uses attributes for evaluation purposes. Giving a test vector for the used boolean
// variables (Example. Variables abc. Testvector 3 means: a=0,b=1,c=1) the resulting values
// of all nodes can be calculated.
// Also Booelan Short Cut Evealuation can be taken into account.
//
// The tree can be printed to the screen or in a file. For easier reading, the printout is turned
// 90� counter clock wise.
//
// The virtual Machine for AST is used to perform operations on ASTs


#include "pch.h"
#include "ast.hpp"
#include "cloption.hpp"

#include <iostream>
#include <iomanip>
#include <cctype>
#include <sstream> 



// Depending on selected program options, we will show the abstract syntax tree with attributes
void VirtualMachineForAST::printTreeStandard(const std::string &source)
{
	// First we want to see the complexity of the AST
	// Big AST may be automatically printed to a file
	// Calculate the requested output stream. Pleass note: This can also be a T
	// Meaning, output to file and cout at the same time is possible
	const uint astSize = narrow_cast<uint>(ast.size());
	const bool predicateForOutputToFile = (astSize > 200);
	OutStreamSelection outStreamSelection(ProgramOption::pastc, predicateForOutputToFile);
	std::ostream& os = outStreamSelection();

	// If program option selected output at all
	if (!outStreamSelection.isNull())
	{
		// Then print headers and footers and the AST (via a subfunction)
		os << "\n------------------ AST   Abstract Syntax Tree      (with attributes) for boolean expression:\n\n'" << source<< "'\n\n------------------\n\n";
		printTree(os);
		os << "\n\n";
	}

}

// Main function to print an AST
// Please note: The given os mayby std::cout, a file or both
// The tree is printed in 90� counter clockwise to have a better usage of the output media
// Line numbers are shown and the operators have also references to these line numbers
// With that it is possible to identify the childs of a node, For eaxmple:
// For the function  "!((a+b)(c+d))"
//// the printout looks like that
// 
// 0                                  ID a(0)
// 1                       OR 0, 2 (0)
// 2                                  ID b(0)
// 3            AND 1, 5 (0)
// 4 NOT 3 (0)                        ID c(0)
// 5                       OR 4, 6 (0)
// 6                                  ID d(0)
// 7
//
// "OR 0, 2 (0)"  means:    Do a OR with the element in line  0 and line 2. The result is (0)
// "AND 1, 5 (0)" means:	Do an AND with the elements in line 1 and line 5.  The result is (0)
//
// Printing is not that easy, because for complex AST theire may be more than one node in a line
// And we can only print line by line. So, we will first print in an internal buffer and 
// if the line is done, we will output the complete line
// 
// The output rows will be calculated in a different routine that sets all properties of an AST
//
void VirtualMachineForAST::printTree(std::ostream& os)
{
	// We will print a tree with a width of maximum 1000 chararcters (columns)
	constexpr uint maxBufSize = 1000;
	// This is the tab size, the distance between 2 levels of an AST
	constexpr long tab = 11;

	// Because we will c-style output, we need to set a 0 terminator at the end of the buffer, 
	// after the last entry for a line has been printed
	uint outputLastPosition{ 0 };

	// Calculate all output strings. Go through all possible rows
	for (row = 0; row < narrow_cast<uint>(ast.size()); ++row)
	{
		// Temporary buffer. Plain old array
		cchar out[maxBufSize + 1];
		// Set complete output buffer tp space ' '
		memset(&out[0], ' ', maxBufSize);
		// Terminate the output buffer with C-Style string terminator 0
		out[maxBufSize] = null<cchar>();

		// Now check all elements in the AST
		for (uint i = 0; i < narrow_cast<uint>(ast.size()); ++i)
		{
			// If the desired row (from the outer loop) matches with the calculated printRow of this node, then we need to do something
			if (row == ast[i].printRow)
			{
				// We will print temporaray data in a stingstream
				std::ostringstream oss;

				// Dpeneding on the number of children, the output will be different
				// 0 = Leaf, 1= Unary operator, 2 = binary operator

				// 0 children, leaf, variable/condition/terminal
				if (NumberOfChildren::zero == ast[i].numberOfChildren)
				{
					// Name of Token (ID or IDNOT), input terminal/variable name, value, evaluated or not
					oss << tokenToCharP(ast[i].tokenWithAttribute.token) << ' ' << ast[i].tokenWithAttribute.inputTerminalSymbol
						<< " (" << ((ast[i].value) ? 1 : 0) << ((ast[i].notEvaluated) ? "x" : "") << ')';
				}
				// 1 child, unary operator NOT
				if (NumberOfChildren::one == ast[i].numberOfChildren)
				{
					// Name of Token (NOT), reference line of child, value, evaluated or not
					oss << tokenToCharP(ast[i].tokenWithAttribute.token) << ' ' << ast[ast[i].childLeftID].printRow
						<< " (" << ((ast[i].value) ? 1 : 0) << ((ast[i].notEvaluated) ? "x" : "") << ')';
				}
				// 2 children, binary operator (OR, XOR, AND) 
				if (NumberOfChildren::two == ast[i].numberOfChildren)
				{
					// Name of Token (NOT), reference line of child left, reference line of child right, value, evaluated or not
					oss << tokenToCharP(ast[i].tokenWithAttribute.token) << ' ' << ast[ast[i].childLeftID].printRow << ',' << ast[ast[i].childRightID].printRow
						<< " (" << ((ast[i].value) ? 1 : 0) << ((ast[i].notEvaluated) ? "x" : "") << ')';
				}
				// Now convert the temporary stringstream to a temporaray std::string
				std::string strTemp{ oss.str() };
				// Calculate the last column  of the output. Lokk for the greatest value in case that there will
				// be 2 strings in the same line
				const uint lastPosition = (tab* ast[i].level) + narrow_cast<uint>(strTemp.size()) + 1;
				// Prevent buffer overflow
				if (lastPosition < maxBufSize)
				{
					// If we have a new biggest column
					if (lastPosition > outputLastPosition)
					{
						// Then remeber that
						outputLastPosition = lastPosition;
					}
					// Copy the temporary string to the output buffer (that is full of spaces) to the calculated column . 
					memcpy(&out[0] + (static_cast<ull>(tab) * static_cast<ull>(ast[i].level)), strTemp.c_str(), strTemp.size());
				}
			}
		}
		// terminate the output buffer with a C-String terminator  0
		out[outputLastPosition] = 0;
		// And put it to the screen and/or to a file
		os << std::setw(6) << row << ' ' << &out[0] << '\n';
	}
}




void VirtualMachineForAST::calculateAstProperties()
{
	uint rootIndex = ast.rbegin()->ownID;
	calculateAstPropertiesRecursive(rootIndex);
}



void VirtualMachineForAST::calculateAstPropertiesRecursive(uint index)
{
	symbolTable.compact();

	ast[index].level = level;


	if (NumberOfChildren::zero == ast[index].numberOfChildren)
	{

		ast[index].tokenWithAttribute.sourceMask = bitMask[symbolTable.normalizedInput[ast[index].tokenWithAttribute.sourceIndex]];
		ast[index].tokenWithAttribute.inputSymbolLowerCase = narrow_cast<cchar>(std::tolower(ast[index].tokenWithAttribute.inputTerminalSymbol));

		ast[index].printRow = row;

		++row;
		++row;
	}
	if (NumberOfChildren::one == ast[index].numberOfChildren)
	{
		ast[ast[index].childLeftID].parentID = ast[index].ownID;


		++level;
		calculateAstPropertiesRecursive(ast[index].childLeftID);
		--level;
		ast[index].printRow = ast[ast[index].childLeftID].printRow + 1;


	}

	if (NumberOfChildren::two == ast[index].numberOfChildren)
	{

		ast[ast[index].childLeftID].parentID = ast[index].ownID;
		ast[ast[index].childRightID].parentID = ast[index].ownID;


		++level;
		calculateAstPropertiesRecursive(ast[index].childLeftID);
		calculateAstPropertiesRecursive(ast[index].childRightID);
		--level;

		ast[index].printRow = (ast[ast[index].childLeftID].printRow + ast[ast[index].childRightID].printRow) / 2;

	}
}



void VirtualMachineForAST::evaluateTree(uint inputValue)
{
	sourceValue = inputValue;
	uint rootIndex = ast.rbegin()->ownID;
	evaluateTreeRecursive(rootIndex);
}
void VirtualMachineForAST::evaluateTreeRecursive(uint index)
{
	AstNode& nodeAtIndex = ast[index];
	const NumberOfChildren numberOfChildren = nodeAtIndex.numberOfChildren;

	if (NumberOfChildren::zero == numberOfChildren)
	{
		switch (nodeAtIndex.tokenWithAttribute.token)
		{
		case Token::ID:
			nodeAtIndex.value = (0U != (sourceValue & nodeAtIndex.tokenWithAttribute.sourceMask));
			nodeAtIndex.notEvaluated = false;
			break;
		case Token::IDNOT:
			nodeAtIndex.value = (0U == (sourceValue & nodeAtIndex.tokenWithAttribute.sourceMask));
			nodeAtIndex.notEvaluated = false;
			break;
		default:
			nodeAtIndex.value = false;
			nodeAtIndex.notEvaluated = true;
			break;
#pragma warning(suppress: 4061)
		}
	}
	else if (NumberOfChildren::one == numberOfChildren)
	{
		evaluateTreeRecursive(nodeAtIndex.childLeftID);
		nodeAtIndex.notEvaluated = false;
		switch (nodeAtIndex.tokenWithAttribute.token)
		{
		case Token::BCLOSE:  //fallthrough
		case Token::END:
			nodeAtIndex.value = ast[nodeAtIndex.childLeftID].value;
			break;
		case Token::NOT:
			nodeAtIndex.value = !ast[nodeAtIndex.childLeftID].value;
			break;
		default:
			nodeAtIndex.value = false;
			nodeAtIndex.notEvaluated = true;
			break;
#pragma warning(suppress: 4061)
		}
	}
	else if (NumberOfChildren::two == numberOfChildren)
	{
		evaluateTreeRecursive(nodeAtIndex.childLeftID);
		evaluateTreeRecursive(nodeAtIndex.childRightID);
		nodeAtIndex.notEvaluated = false;

		switch (nodeAtIndex.tokenWithAttribute.token)
		{
		case Token::AND:
			nodeAtIndex.value = ast[nodeAtIndex.childLeftID].value && ast[nodeAtIndex.childRightID].value;
			break;
		case Token::OR:
			nodeAtIndex.value = ast[nodeAtIndex.childLeftID].value || ast[nodeAtIndex.childRightID].value;
			break;
		case Token::XOR:
			nodeAtIndex.value = ast[nodeAtIndex.childLeftID].value ^ ast[nodeAtIndex.childRightID].value;
			break;

		default:
			nodeAtIndex.value = false;
			nodeAtIndex.notEvaluated = true;
			break;
#pragma warning(suppress: 4061)
		}
	}

	bool isNotEvaluated{ false };


	// Boolean short circuit evaluation
	switch (nodeAtIndex.tokenWithAttribute.token)
	{
	case Token::AND:
		if (!ast[nodeAtIndex.childLeftID].value)
		{
			// Right side of tree is notEvaluated
			isNotEvaluated = true;
		}
		break;
	case Token::OR:
		if (ast[nodeAtIndex.childLeftID].value)
		{
			// Right side of tree is notEvaluated
			isNotEvaluated = true;
		}
		break;
#pragma warning(suppress: 4061)
#pragma warning(suppress: 4062)
	}
	if (isNotEvaluated)
	{
		// ***************************************************************************
		// ***************************************************************************
		// ***************************************************************************
		// ***************************************************************************
		if (programOption.option[ProgramOption::bse].optionSelected)
		{
			setSubTreetoMasked(nodeAtIndex.childRightID);
		}
	}

}
void VirtualMachineForAST::setSubTreetoMasked(uint index)
{
	ast[index].notEvaluated = true;
	if (NumberOfChildren::zero == ast[index].numberOfChildren)
	{
	}
	else if (NumberOfChildren ::one == ast[index].numberOfChildren)
	{
		setSubTreetoMasked(ast[index].childLeftID);
	}
	else if (NumberOfChildren::two == ast[index].numberOfChildren)
	{
		setSubTreetoMasked(ast[index].childLeftID);
		setSubTreetoMasked(ast[index].childRightID);
	}
}

