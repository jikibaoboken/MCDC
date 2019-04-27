
// This software implements 2 compiler that take a boolean
// expression as input and produce 2 outputs
//
// 1. An object code that can be run be a virtual machine
// 2. An abstract syntax tree (AST) for calculation of MCDC test pairs
//
//
// The compiler frontent, the scanner and the parser are the same
// The "code generator" makes the difference.
//
// The input of the code generator is the output of the parser.
// The parser, implemented as shift reduce parser, reads a tokrn
// shifts it on the parse stack, and tries to match it with a handle
// given in the grammar. The parsesatck is then reduced according
// to the size of the handle and this code generator will be
// invoked with the handle elements as parameter.
// The grammar, defined here for the boolean expressions, defines
// handles with 1, 2 or 3 elements. Therefore the code generator
// implements 3 overloaded "geneate" functions, that will be used
// for the respective handle.
//
// The first code generator builds an object file with so called
// 3 byte opcodes. There is always an operation, 1 or 2 source
// registers and a target or result register.
// 
// A virtual machine loads the generated object code and executes it.
//
//
// The 2nd code geenrator builds an abstract syntax tree AST. This is 
// implemented as a vector with internal linkage fields. See ast.h
// for further desctriptions
//
// Also the AST can be "executed" to generate results.
//
// Both code generators are derived from a common base clase, because
// they share the same compiler front end.

#include "pch.h"

#include "symboltable.hpp"
#include "codegenerator.hpp"

#include <iostream>
#include <iomanip>
#include <cctype>


// The symbol table stores the variables present in the boolean source expression
// Symbols can be a-z and (negated) A-Z. a-z are 26 varaibles. We cannot handle 26 variables
// But still we will allow symbol usage with gaps:  for example:  a+d+z
// 3 variables, but with gaps
// The following function will eliminate the gaps
void SymbolTable::compact()
{
	
	const uint symbolCount{ numberOfSymbols() };

	normalizedInput.clear();
	if (symbolCount  > 0)
	{
		// ALso make a the Most significant bit
		// So if we set a source 2 value for variables ab, a will be 1 and b will be 0
		// So start with upper bound of symbol
		uint symbolCounter{ symbolCount - 1 };
		
		for (const cchar c : symbol)
		{
			// Now simply assign consecutive numbers
			normalizedInput[static_cast<uint>(c - 'a')] = symbolCounter;
			--symbolCounter;
		}
	}
}


// The codegenerator assign virtual machine registers to store temporary values
// This is simply a vector of boolean values.
// If a new temporary register is needed, then the following routine tries to get
// one. Either a new one, or it will reuse an existing one that will not be used
// any longer. Registers that do not need to be used any longer, then they can be
// free'd again. This is a kind of optimization. 
uint CodeGeneratorForVM::getNextAvailableRegister()
{
	uint result{ 0 };	// Resulting register number
	bool existingRegisterFound{ false }; // If we can reuse an existing register

	// Go through all available registers
	uint i = 0U;
	while (i < availabeRegister.size())
	{
		// If the register is available for reuse
		if (availabeRegister[i])
		{
			// Now it is no longer available
			availabeRegister[i] = false;
			// And we found something
			existingRegisterFound = true;
			// Do not increment i. Jump out of the loop. With that, i points to the just found variable
			break;  
		}
		// CHeck next existing register
		++i;
	}
	// If there was no register availabe, that could have been reused
	if (!existingRegisterFound)
	{
		// Then we simply create a new temporary register
		availabeRegister.push_back(false);
	}
	// variable i either points to an existing register (to be reused) or at 1+the end of the vector
	result = i;
	// This is the new register to be used
	return result;
}

// Set a register to be available again
void CodeGeneratorForVM::setRegisterToAvailable(uint index)
{
	if (index < availabeRegister.size())  // Boundary check
	{
		// Can be reused
		availabeRegister[index] = true;
	}
}



// This are the actual code generation routines.
//
// There is one routine for operations with 1,2 or 3 parameters
// The first parameter is always the index of the production of the grammar
// And then we will have 1,2 or 3 tokens.

// Operation with 1 parameter, which is at the moment ID or IDNOT
// This results from the handle length in the grammar
TokenWithAttribute CodeGeneratorForVM::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1)
{
	
	OpCodeLine opCodeLine;		// Resulting opCodeLine
	TokenWithAttribute result;	// New token: non-terminal. Will be expression
	
	const Token token = production.operationIdentifier;

	// If it is an ID. So a variable in the boolean expression
	if (Token::ID == token)
	{
		// Lower case letter (a positive, none negated value)
		if (tokenWithAttribute1.sourceIndex < 26)
		{
			opCodeLine.token = Token::ID; // Operation is normal positive ID
			opCodeLine.parameter1 = tokenWithAttribute1.sourceIndex;  // Store the index of the input 
		}
		else
		{
			opCodeLine.token = Token::IDNOT; // Operation is negative ID
			opCodeLine.parameter1 = tokenWithAttribute1.sourceIndex - 26; // Store the index of the negated input 
		}

		// The Source value will be stored in the next available register
		opCodeLine.parameter2 = getNextAvailableRegister();
		// This is for output purposes 
		opCodeLine.parameter3 = static_cast<uint>(narrow_cast<cchar>(std::tolower(tokenWithAttribute1.inputTerminalSymbol)));

		// And we will add this symbol to the symbol table
		objectCode.symbolTable.symbol.insert((narrow_cast<cchar>(std::tolower(tokenWithAttribute1.inputTerminalSymbol))));

		
		// The result is always the non-terminal EXPR   (expression)
		result.token = Token::EXPR;
		// Number of destination register
		result.sourceIndex = opCodeLine.parameter2;
	}
	// Add the new opcode to the object code.
	objectCode.add(std::move(opCodeLine));
	return result;
}


// Operation with 2 parameter, which is at the moment NOT, END and AND (the concatenated form. So "ab" and not "a&&b")
// This results from the handle length in the grammar
TokenWithAttribute CodeGeneratorForVM::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1, TokenWithAttribute &tokenWithAttribute2)
{
	OpCodeLine opCodeLine; 		// Resulting opCodeLine
	TokenWithAttribute result;	// New token: non-terminal. Will be expression
	
	const Token token = production.operationIdentifier;
	
	// For NOT
	if (Token::NOT == token)
	{
		opCodeLine.token = token;
		// Simply copy the value.
		opCodeLine.parameter1 = tokenWithAttribute1.sourceIndex;
		// New destination register
		opCodeLine.parameter2 = getNextAvailableRegister();
		// Source regsiter no longer needed
		setRegisterToAvailable(tokenWithAttribute1.sourceIndex);

		// The result is always the non-terminal EXPR   (expression)
		result.token = Token::EXPR;
		result.sourceIndex = opCodeLine.parameter2;
	}
	// AND in concatenated form
	else if (Token::AND == token)
	{
		opCodeLine.token = token;
		// Operand 1
		opCodeLine.parameter1 = tokenWithAttribute1.sourceIndex;
		// Operand 2
		opCodeLine.parameter2 = tokenWithAttribute2.sourceIndex;
		// Destination for Result of AND operation
		opCodeLine.parameter3 = getNextAvailableRegister();
		// Both source registers no longer needed
		setRegisterToAvailable(tokenWithAttribute1.sourceIndex);
		setRegisterToAvailable(tokenWithAttribute2.sourceIndex);

		// The result is always the non-terminal EXPR   (expression)
		result.token = Token::EXPR;
		result.sourceIndex = opCodeLine.parameter3;
	}
	else if (Token::END == token)
	{
		// End of program. Basically 
		opCodeLine.token = token;
		opCodeLine.parameter1 = tokenWithAttribute2.sourceIndex;
		setRegisterToAvailable(tokenWithAttribute2.sourceIndex);

		// The result is always the non-terminal EXPR   (expression)
		result.token = Token::END;
		//result.sourceIndex = -1;
	}
	objectCode.add(std::move(opCodeLine));
	return result;
}



TokenWithAttribute CodeGeneratorForVM::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1, TokenWithAttribute &tokenWithAttribute2, TokenWithAttribute &tokenWithAttribute3)
{
	OpCodeLine opCodeLine;
	TokenWithAttribute result;
	const Token token = production.operationIdentifier;
	


	if (Token::BCLOSE == token)
	{
		//setRegisterToAvailable(tokenWithAttribute2.sourceIndex);
		result.token = Token::EXPR;
		result.sourceIndex = tokenWithAttribute2.sourceIndex;
	}
	else if ((Token::OR == token) || (Token::AND == token) || (Token::XOR == token))
	{
		opCodeLine.token = token;
		opCodeLine.parameter1 = tokenWithAttribute3.sourceIndex;
		opCodeLine.parameter2 = tokenWithAttribute1.sourceIndex;
		opCodeLine.parameter3 = getNextAvailableRegister();
		setRegisterToAvailable(tokenWithAttribute3.sourceIndex);
		setRegisterToAvailable(tokenWithAttribute1.sourceIndex);

		result.token = Token::EXPR;
		result.sourceIndex = opCodeLine.parameter3;
	objectCode.add(std::move(opCodeLine));
	}

	return result;
}


// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************
// *************************************************************************************************************************

TokenWithAttribute CodeGeneratorForAST::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1)
{

	AstNode astElement;
	TokenWithAttribute result;

	const Token token = production.operationIdentifier;

	astElement.tokenWithAttribute = tokenWithAttribute1;
	if (Token::ID == token)
	{
		if (tokenWithAttribute1.sourceIndex < 26)
		{
			astElement.tokenWithAttribute.token = Token::ID;
			astElement.tokenWithAttribute.sourceIndex= tokenWithAttribute1.sourceIndex;
		}
		else
		{
			astElement.tokenWithAttribute.token = Token::IDNOT;
			astElement.tokenWithAttribute.sourceIndex = tokenWithAttribute1.sourceIndex-26;
		}
		//astElement.tokenWithAttribute.inputTerminalSymbol = tokenWithAttribute1.sourceIndex;
		astElement.ownID = currentIndex;

		ast.addSymbol(tokenWithAttribute1.inputTerminalSymbol);

		result.token = Token::EXPR;
		result.sourceIndex = currentIndex++;
	}

	ast.add(std::move(astElement));

	return result;
}

void VirtualMachineForAST::addSymbol(cchar s)
{
	symbolTable.symbol.insert((narrow_cast<cchar>(std::tolower(s))));
}

TokenWithAttribute CodeGeneratorForAST::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1, TokenWithAttribute &tokenWithAttribute2)
{
	

	AstNode astElement;
	TokenWithAttribute result;
	const Token token = production.operationIdentifier;

	if (Token::NOT == token)
	{
		astElement.tokenWithAttribute = tokenWithAttribute1;
		astElement.tokenWithAttribute.token = Token::NOT;
		astElement.numberOfChildren = NumberOfChildren::one;
		astElement.childLeftID = tokenWithAttribute1.sourceIndex;
		astElement.ownID = currentIndex;

		result.token = Token::EXPR;
		result.sourceIndex = currentIndex++;
		ast.add(std::move(astElement));
	}
	else if (Token::AND == token)
	{
		astElement.tokenWithAttribute = tokenWithAttribute1;
		astElement.tokenWithAttribute.token = Token::AND;
		astElement.numberOfChildren = NumberOfChildren::two;
		astElement.childLeftID = tokenWithAttribute2.sourceIndex;
		astElement.childRightID = tokenWithAttribute1.sourceIndex;
		astElement.ownID = currentIndex;

		result.token = Token::EXPR;
		result.sourceIndex = astElement.ownID = currentIndex++;
		ast.add(std::move(astElement));
	}
	else if (Token::END == token)
	{

		result.token = Token::END;
		result.sourceIndex = ASTNoLinkedElement;
	}

	return result;
}

TokenWithAttribute CodeGeneratorForAST::generate(const Production &production, TokenWithAttribute &tokenWithAttribute1, TokenWithAttribute &tokenWithAttribute2, TokenWithAttribute &tokenWithAttribute3)
{
	
	AstNode astElement;

	TokenWithAttribute result;
	const Token token = production.operationIdentifier;



	if (Token::BCLOSE == token)
	{
		result.token = Token::EXPR;
		result.sourceIndex = tokenWithAttribute2.sourceIndex;
	}
	else if ((Token::OR == token) || (Token::AND == token) || (Token::XOR == token))
	{

		astElement.tokenWithAttribute = tokenWithAttribute1;
		astElement.tokenWithAttribute.token = token;
		astElement.numberOfChildren = NumberOfChildren::two;
		astElement.childLeftID = tokenWithAttribute3.sourceIndex;
		astElement.childRightID = tokenWithAttribute1.sourceIndex;
		astElement.ownID = currentIndex;



		result.token = Token::EXPR;
		result.sourceIndex = currentIndex++;

		ast.add(std::move(astElement));	
	}

	return result;
}
