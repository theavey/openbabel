/**********************************************************************
Copyright (C) 2011 by Noel O'Boyle

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/

#include <openbabel/babelconfig.h>
#include <openbabel/obmolecformat.h>
#include <openbabel/op.h>
#include <openbabel/depict/depict.h>
#include <openbabel/depict/cairopainter.h>
#include <openbabel/alias.h>

using namespace std;



namespace OpenBabel
{

class PNG2Format : public OBMoleculeFormat
{
public:
  //Register this format type ID in the constructor
  PNG2Format() : _ncols(0), _nrows(0), _nmax(0)
  {
    OBConversion::RegisterFormat("png2",this);
  }

  virtual const char* Description() //required
  {
    return
    "PNG2 format\n"
    "2D depiction of a single molecule as a .png file\n\n"

    "The PNG2 format is used 'behind the scenes' by the :ref:`PNG format<PNG_2D_depiction>`\n"
    "if generating image files, and the best way to use it is\n"
    "actually through the PNG format. While it possible to generate\n"
    "a :file:`.png` file directly using the PNG2 format as follows...::\n\n"
    "  obabel -:\"CC(=O)Cl\" -opng2 -O mymol.png\n\n"
    "...it is much better to generate it using the PNG format\n"
    "as this allows you to embed a chemical structure in the\n"
    ":file:`.png` file header which you can later extract::\n\n"
    "  $ obabel -:\"CC(=O)Cl\" -O mymol.png -xO smi\n"
    "  $ obabel mymol.png -osmi\n"
    "  CC(=O)Cl\n\n"

    "The PNG2 format uses the Cairo library to generate the\n"
    ":file:`.png` files.\n"
    "If Cairo was not found when Open Babel was compiled, then\n"
    "this format will be unavailable. However, it will still be possible\n"
    "to use the PNG format to read :file:`.png` files if they contain\n"
    "embedded information.\n\n"

    ".. seealso::\n\n"

    "    :ref:`PNG_2D_depiction`\n\n"

    "Write Options e.g. -xp 500\n"
    " p <pixels> image size, default 300\n"
    " w <pixels> image width, default is image size (p)\n"
    " h <pixels> image height, default is image size (p)\n"
      " c# number of columns in table\n"
      " r# number of rows in table\n"
      " N# max number objects to be output\n"
      " u no element-specific atom coloring\n"
      "    Use this option to produce a black and white diagram\n"
      " U do not use internally-specified color\n"
      "    e.g. atom color read from cml or generated by internal code\n"
      " C do not draw terminal C (and attached H) explicitly\n"
      "    The default is to draw all hetero atoms and terminal C explicitly,\n"
      "    together with their attched hydrogens.\n"
      " a draw all carbon atoms\n"
      "    So propane would display as H3C-CH2-CH3\n"
      " d do not display molecule name\n"
      " s use asymmetric double bonds\n"
      " t use thicker lines\n"
      " A display aliases, if present\n"
      "    This applies to structures which have an alternative, usually\n"
      "    shorter, representation already present. This might have been input\n"
      "    from an A or S superatom entry in an sd or mol file, or can be\n"
      "    generated using the --genalias option. For example::\n \n"

      "      obabel -:\"c1cc(C=O)ccc1C(=O)O\" -O out.png\n"
      "             --genalias -xA\n \n"

      "    would add a aliases COOH and CHO to represent the carboxyl and\n"
      "    aldehyde groups and would display them as such in the svg diagram.\n"
      "    The aliases which are recognized are in data/superatom.txt, which\n"
      "    can be edited.\n"
      "\n"
    ;
  };


  virtual unsigned int Flags()
  {
      return NOTREADABLE | WRITEBINARY;
  };
  bool WriteChemObject(OBConversion* pConv);
  bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

  private:
    int _ncols, _nrows, _nmax;
    vector<OBBase*> _objects;
    CairoPainter _cairopainter;
};
  ////////////////////////////////////////////////////

//Make an instance of the format class
PNG2Format thePNG2Format;

/////////////////////////////////////////////////////////////////

bool PNG2Format::WriteChemObject(OBConversion* pConv) // Taken from svgformat.cpp
{
  //Molecules are stored here as pointers to OBBase objects, which are not deleted as usual.
  //When there are no more they are sent to WriteMolecule.
  //This allows their number to be determined whatever their source
  //(they may also have been filtered), so that the table can be properly dimensioned.

  OBBase* pOb = pConv->GetChemObject();

  if(pConv->GetOutputIndex()<=1)
  {
    _objects.clear();
    _nmax=0;

    pConv->AddOption("pngwritechemobject"); // to show WriteMolecule that this function has been called
    const char* pc = pConv->IsOption("c");
    const char* pr = pConv->IsOption("r");
    if(pr)
      _nrows = atoi(pr);
    if(pc)
      _ncols = atoi(pc);
    if(pr && pc) // both specified: fixes maximum number objects to be output
      _nmax = _nrows * _ncols;

    //explicit max number of objects
    const char* pmax =pConv->IsOption("N");
    if(pmax)
      _nmax = atoi(pmax);
  }

  OBMoleculeFormat::DoOutputOptions(pOb, pConv);

  //save molecule
  _objects.push_back(pOb);

  bool ret=true;
  //Finish if no more input or if the number of molecules has reached the allowed maximum(if specified)
  bool nomore = _nmax && (_objects.size()==_nmax);
  if((pConv->IsLast() || nomore))
  {
    int nmols = _objects.size();
    //Set table properties according to the options and the number of molecules to be output
    if(!(nmols==0 ||                      //ignore this block if there is no input or
         (_nrows && _ncols) ||            //if the user has specified both rows and columns or
         (!_nrows && !_ncols) && nmols==1)//if neither is specified and there is one output molecule
      )
    {
      if(!_nrows && !_ncols ) //neither specified
      {
        //assign cols/rows in square
        _ncols = (int)ceil(sqrt(((double)nmols)));
      }

      if(_nrows)
        _ncols = (nmols-1) / _nrows + 1; //rounds up
      else if(_ncols)
        _nrows = (nmols-1) / _ncols + 1;
    }

    //output all collected molecules
    int n=0;

    vector<OBBase*>::iterator iter;
    for(iter=_objects.begin(); ret && iter!=_objects.end(); ++iter)
    {
      //need to manually set these to mimic normal conversion
      pConv->SetOutputIndex(++n);
      pConv->SetLast(n==_objects.size());

      ret=WriteMolecule(*iter, pConv);

    }

    //delete all the molecules
    for(iter=_objects.begin();iter!=_objects.end(); ++iter)
      delete *iter;

    _objects.clear();
    _nmax = _ncols = _nrows = 0;
  }
  //OBConversion decrements OutputIndex when returns false because it thinks it is an error
  //So we compensate.
  if(!ret || nomore)
    pConv->SetOutputIndex(pConv->GetOutputIndex()+1);
  return ret && !nomore;
}


////////////////////////////////////////////////////////////////

bool PNG2Format::WriteMolecule(OBBase* pOb, OBConversion* pConv)
{
  OBMol* pmol = dynamic_cast<OBMol*>(pOb);
  if(pmol==NULL)
      return false;

  ostream& ofs = *pConv->GetOutStream();

  OBMol workingmol(*pmol); // Copy the molecule

  if (!pConv->IsOption("pngwritechemobject"))
  { //If WriteMolecule called directly, e.g. from OBConversion::Write()
    _nmax = _nrows = _ncols = 1;
    pConv->SetLast(true);
    pConv->SetOutputIndex(1);
  }

  //*** Coordinate generation ***
  //Generate coordinates only if no existing 2D coordinates
  if(!workingmol.Has2D(true))
  {
    OBOp* pOp = OBOp::FindType("gen2D");
    if(!pOp)
    {
      obErrorLog.ThrowError("PNG2Format", "gen2D not found", obError, onceOnly);
      return false;
    }
    if(!pOp->Do(&workingmol))
    {
      obErrorLog.ThrowError("PNG2Format", string(workingmol.GetTitle()) + "- Coordinate generation unsuccessful", obError);
      return false;
    }
  }
  if(!workingmol.Has2D() && workingmol.NumAtoms()>1)
  {
    string mes("Molecule ");
    mes += workingmol.GetTitle();
    mes += " needs 2D coordinates to display in PNG2format";
    obErrorLog.ThrowError("PNG2Format", mes, obError);
    return false;
  }

  const char* pp = pConv->IsOption("p");
  int size  = pp ? atoi(pp) : 300;

  pp = pConv->IsOption("w"); // Default values for width and height are the size
  int width  = pp ? atoi(pp) : size;
  pp = pConv->IsOption("h");
  int height  = pp ? atoi(pp) : size;

  string text;
  if(!pConv->IsOption("d"))
    text = pmol->GetTitle();
  else
    text = pmol->GetTitle();
  _cairopainter.SetTitle(text);

  if(pConv->GetOutputIndex()==1) {
    _cairopainter.SetWidth(width);
    _cairopainter.SetHeight(height);
    _cairopainter.SetTableSize(_nrows, _ncols);
  }
  _cairopainter.SetIndex(pConv->GetOutputIndex());

  OBDepict depictor(&_cairopainter);

  // The following options are all taken from svgformat.cpp
  if(!pConv->IsOption("C"))
    depictor.SetOption(OBDepict::drawTermC);
  if(pConv->IsOption("a"))
    depictor.SetOption(OBDepict::drawAllC);

  if(pConv->IsOption("A"))
  {
    AliasData::RevertToAliasForm(workingmol);
    depictor.SetAliasMode();
  }
  if(pConv->IsOption("t"))
    _cairopainter.SetPenWidth(4);
  else
    _cairopainter.SetPenWidth(1);

  //No element-specific atom coloring if requested
  if(pConv->IsOption("u"))
    depictor.SetOption(OBDepict::bwAtoms);
  if(!pConv->IsOption("U"))
    depictor.SetOption(OBDepict::internalColor);
  if(pConv->IsOption("s"))
    depictor.SetOption(OBDepict::asymmetricDoubleBond);

  // Draw it!
  depictor.DrawMolecule(&workingmol);

  // Write it out
  if (pConv->IsLast())
    _cairopainter.WriteImage(ofs);

  return true; //or false to stop converting
}

} //namespace OpenBabel

