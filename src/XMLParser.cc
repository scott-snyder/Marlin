
#include "marlin/XMLParser.h"
#include "marlin/Exceptions.h"
#include "marlin/tinyxml.h"

#include <algorithm>

#include <sstream>

namespace marlin{

  // open steering file with processor names 
  XMLParser::XMLParser( const std::string&  fileName) :
    _current(0) , _fileName( fileName ) {
  }
  
  XMLParser::~XMLParser(){
  }
  
  void XMLParser::parse(){
    
    
    _doc = new TiXmlDocument ;
    bool loadOkay = _doc->LoadFile(_fileName  ) ;
    
    if( !loadOkay ) {

      std::stringstream str ;
      
      str  << "XMLParser::parse error in file [" << _fileName 
	   << ", row: " << _doc->ErrorRow() << ", col: " << _doc->ErrorCol() << "] : "
	   << _doc->ErrorDesc() ;
      
      throw ParseException( str.str() ) ;
    }
    
//     TiXmlHandle docHandle( _doc );
    
    TiXmlElement* root = _doc->RootElement();
    if( root==0 ){

      throw ParseException(std::string( "XMLParser::parse : no root tag <marlin>...</marlin> found in  ") 
			   + _fileName  ) ;
    }
    
    TiXmlNode* section = 0 ;
    

    _map[ "Global" ] = new StringParameters() ;

    section = root->FirstChild("global")  ;

    if( section != 0  ){
      _current =  _map[ "Global" ] ;
      parametersFromNode( section ) ;

    }  else {

      throw ParseException(std::string( "XMLParser::parse : no <global/> section found in  ") 
			   + _fileName  ) ;
    }
    

// create global parameter ActiveProcessors from <execute> section
//     TiXmlElement activeProcessorItem( "parameter" );
//     activeProcessorItem.SetAttribute( "name", "ActiveProcessors" );
    
    std::vector<std::string> activeProcs ;
    activeProcs.push_back("ActiveProcessors") ;

    std::vector<std::string> procConds ;
    procConds.push_back("ProcessorConditions") ;

    section = root->FirstChild("execute")  ;
    if( section != 0  ){
      
      // preprocess groups: replace with <processor/> tags
      replacegroups(  section )  ;

      // process processor conditions:
      processconditions( section , "" ) ;

//       std::cout <<  " execute after group replacement : " << *section << std::

      TiXmlNode* proc = 0 ;
      while( ( proc = section->IterateChildren( "processor", proc ) )  != 0  ){

	activeProcs.push_back( getAttribute( proc, "name") ) ;

	std::string condition(  getAttribute( proc,"condition")  ) ;

	if( condition.size() == 0 ) 
	  condition += "true" ;

	procConds.push_back( condition  ) ;

      }

    } else {

      throw ParseException(std::string( "XMLParser::parse : no <execute/> section found in  ") 
			   + _fileName  ) ;
    }

    _current->add( activeProcs ) ;
    _current->add( procConds ) ;



    // preprocess groups: ---------------------------------------------------------------------------------
    // simply copy all group parameters to the processors
    // and then copy the processors to the root node <Marlin>
    while( (section = root->IterateChildren( "group", section ) )  != 0  ){
      
      std::vector<TiXmlNode*> groupParams ;

      TiXmlNode* param = 0 ;
      while( ( param = section->IterateChildren( "parameter" , param ) )  != 0  ){
	groupParams.push_back( param->Clone() ) ;
      }

      TiXmlNode* proc = 0 ;
      while( ( proc = section->IterateChildren( "processor" , proc ) )  != 0  ){
	
	for( std::vector<TiXmlNode*>::iterator it = groupParams.begin() ; it != groupParams.end() ; it++){
	  proc->InsertEndChild( **it ) ;
	}
	std::auto_ptr<TiXmlNode> clone( proc->Clone() ) ;
	root->InsertBeforeChild( section , *clone  ) ;   // FIXME: memory mngmt. ?
      }

      root->RemoveChild( section ) ;

      for( std::vector<TiXmlNode*>::iterator it = groupParams.begin() ; it != groupParams.end() ; it++){
	delete *it ;
      }
    }
    
    
    // process processor sections -------------------------------------------------------------------------
    while( (section = root->IterateChildren("processor",  section ) )  != 0  ){
      
//       std::cout << " processor found: " << section->Value() << std::endl ;
      
      _current = new StringParameters() ;
      
      _map[ getAttribute( section, "name") ] =  _current ;
      

      // add ProcessorType to processor parameters
      std::vector<std::string> procType(2)  ;
      procType[0] = "ProcessorType" ;
      procType[1] =   getAttribute( section, "type")  ;
      _current->add( procType ) ;

      parametersFromNode( section ) ;
    }


    // DEBUG:
    _doc->SaveFile( "debug.xml" ) ;

   }
  
  const char* XMLParser::getAttribute( TiXmlNode* node , const std::string& name ){
 
    TiXmlElement* el = node->ToElement() ;
    if( el == 0 ) 
      throw ParseException("XMLParser::getAttribute not an XMLElement " ) ;

    const char* at = el->Attribute( name.c_str() )  ;

    if( at == 0 ){

      std::stringstream str ;
      str  << "XMLParser::getAttribute missing attribute \"" << name 
	   << "\" in element <" << el->Value() << "/> in file " << _fileName  ;
      throw ParseException( str.str() ) ;
    }

    return at ;

  }  


  void XMLParser::parametersFromNode(TiXmlNode* section) { 
    
    TiXmlNode* par = 0 ;
    while( ( par = section->IterateChildren( "parameter", par ) )  != 0  ){
      
      
//        std::cout << " parameter found : " << par->ToElement()->Attribute("name") << std::endl ;
      
      std::vector<std::string> tokens ;
      tokens.push_back( std::string( getAttribute( par, "name" )  )  ) ; // first token is parameter name 
      LCTokenizer t( tokens ,' ') ;
      
      std::string inputLine("") ;
      
      try{  inputLine = getAttribute( par , "value" )  ; 
      }      
      catch( ParseException ) {

	if( par->FirstChild() )
	  inputLine =  par->FirstChild()->Value() ;
      }
//       if( par->ToElement()->Attribute("value") != 0 ) {
// 	inputLine = par->ToElement()->Attribute("value") ;
//       }
//       else if( par->FirstChild() ) {
// 	inputLine =  par->FirstChild()->Value() ;
//       }
      
//        std::cout << " values : " << inputLine << std::endl ;
    
       std::for_each( inputLine.begin(),inputLine.end(), t ) ; 

//        for( StringVec::iterator it = tokens.begin() ; it != tokens.end() ; it++ ){
//  	std::cout << "  " << *it ;
//        } 
//        std::cout << std::endl ;
      
      _current->add( tokens ) ;

    }
  }
  
  //   TiXmlElement* child2 = docHandle.FirstChild( "Document" ).FirstChild( "Element" ).Child( "Child", 1 ).Element();
  //   if ( child2 ){}

  StringParameters* XMLParser::getParameters( const std::string& sectionName ) const {
    
//     for( StringParametersMap::iterator iter = _map.begin() ; iter != _map.end() ; iter++){
//       //     std::cout << " parameter section " << iter->first 
//       // 	      << std::endl 
//       // 	      << *iter->second 
//       // 	      << std::endl ;
//     }
    
    return _map[ sectionName ] ;
  }
  

  void XMLParser::processconditions( TiXmlNode* current , const std::string& aCondition ) {
    
    std::string condition ;
    // put parentheses around compound expressions 
    if( aCondition.find('&') != std::string::npos  ||  aCondition.find('|') != std::string::npos ) 
      condition = "(" + aCondition + ")" ;
    else
      condition = aCondition ;

    TiXmlNode* child = 0 ;
    while( ( child = current->IterateChildren( "if" , child )  )  != 0  ){
      
      processconditions( child , getAttribute( child, "condition") ) ;  
    }

    while( ( child = current->IterateChildren( "processor" , child )  )  != 0  ) {
      

      if(  child->ToElement()->Attribute("condition") == 0 ) {
	
	child->ToElement()->SetAttribute("condition" ,  condition ) ;

      } else {

	std::string cond( child->ToElement()->Attribute("condition") ) ; 

	if( cond.size() > 0 && condition.size() ) 
	  cond += " && " ;

	cond += condition ;
	
	child->ToElement()->SetAttribute("condition" ,  cond ) ;

      }
       

      if( std::string( current->Value() ) != "execute" ) {

	// unless we are already in the top node (<execute/>) we have to move all processors up

	TiXmlNode* parent = current->Parent() ;

	std::auto_ptr<TiXmlNode> clone( child->Clone() ) ;

	parent->InsertBeforeChild(  current , *clone ) ;  

      }

    }
    // remove the current <if/> node
    if( std::string( current->Value() ) != "execute" ) {
      TiXmlNode* parent = current->Parent() ;
      parent->RemoveChild( current ) ;
    }    
    
  }



  void XMLParser::replacegroups(TiXmlNode* section) {
    
    TiXmlElement* root = _doc->RootElement()  ;
    if( root==0 ){
      std::cout << "XMLParser::parse : no root tag <marlin>...</marlin> found ! " << std::endl ;
      return ;
    }

    TiXmlNode* child = 0 ;
    while( ( child = section->IterateChildren( child ) )  != 0  ){
      
      if( std::string( child->Value() )  == "group" ) {
	
	// find group definition in root node 
	TiXmlNode* group = findElement( root, "group", "name" , getAttribute( child, "name") ) ;
	
	if( group != 0 ) {
	  
	  TiXmlNode* sub = 0 ;
	  while( ( sub = group->IterateChildren( "processor" , sub ) )  != 0  ){
	    
	    // insert <processor/> tag
	    TiXmlElement item( "processor" );
	    item.SetAttribute( "name",  getAttribute( sub, "name") ) ;

	    section->InsertBeforeChild( child , item ) ;
	      
// 	      std::cout << " inserting processor tag for group : " << item.Value() << ", " 
// 			<< item.Attribute("name") << std::endl ;
	  }

	  section->RemoveChild( child ) ; 

	} else
	  
	  std::cout << " XMLParser::parse - group not found : " <<  child->ToElement()->Attribute("name") << std::endl ;

      } else  if( std::string( child->Value() )  == "if" )  {  // other element, e.g. <if></if>

	replacegroups( child ) ;

      }
    }
  }
  
  
  TiXmlNode* XMLParser::findElement( TiXmlNode* node , const std::string& type, 
			  const std::string& attribute, const std::string& value ) {
    
    TiXmlNode* child = 0 ;
    bool elementFound  = false ;
    
    while( (child = node->IterateChildren( type , child ) )  != 0  ){
      
      if( std::string( *child->ToElement()->Attribute( attribute ) ) == value ) { 
	elementFound = true ;
	break ;
      }
    }
    if( ! elementFound ) 
      child = 0 ;
    
    return child ;
  }  
  
}  // namespace marlin


