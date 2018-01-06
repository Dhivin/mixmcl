#include "mixmcl/kcgrid.h"

using namespace boost;
using namespace std;
using namespace paramio;
using namespace dataio;
KCGrid::KCGrid(size_t X, size_t Y, size_t D, string& para_file)
:X(X), Y(Y), D(D), max_size_(X*Y*D), data_count_(0)
{
  scoped_ptr<ParamIn> param(new ParamIn(para_file));
  if(!param->readAllLines())
  {
    stringstream ss;
    ss << "KCGrid::KCGrid(..., string&) cannot read parameter file named \"";
    ss << para_file.c_str() << '\"' << endl;
    throw runtime_error(ss.str());
  }
  assignLimits(param->map_);
  convert(param->map_);
}

KCGrid::KCGrid(size_t X, size_t Y, size_t D
      ,map<string, any>& m)
:X(X), Y(Y), D(D), max_size_(X*Y*D), data_count_(0)
{
  if(m.size()==0)
    throw runtime_error("wrong parameters for KCGrid::KCGrid(..., map<string, any>)");
  assignLimits(m);
  convert(m);
}

KCGrid::KCGrid(size_t X, size_t Y, size_t D
      , ParamIn& param)
:X(X), Y(Y), D(D), max_size_(X*Y*D), data_count_(0)
{
  if(param.map_.size()==0)
    if(!param.readAllLines())
      throw runtime_error("cannot read parameters in KCGrid::KCGrid(..., ParamIn)");
  assignLimits(param.map_);
  convert(param.map_);
}

inline void KCGrid::VI2GI(size_t index, vector<size_t>& gi)
{
  gi.resize(3);
  gi[0] = index/(Y*D);
  gi[1] = (index - (gi[0]*Y*D))/D;
  gi[2] = (index - (gi[0]*Y*D) - (gi[1]*D)) % D;
}


inline size_t KCGrid::GI2VI(size_t x, size_t y, size_t d)
{
  size_t idx = x*Y*D + y*D + d;
  //vector<size_t> gi;
  //VI2GI(idx, gi);
  //assert(gi[0]==x);
  //assert(gi[1]==y);
  //assert(gi[2]==d);
  if(idx >= max_size_)
  {
    stringstream ss;
    ss << "out of range in KCGrid::GI2VI(size_t, size_t, size_t)" << endl;
    ss << "reading " << idx << "-th element" << endl;
    ss << "discrete  coordinate: " << x << " " << y << " " << d << endl;
    throw out_of_range(ss.str());
  }
  return idx;
}

inline size_t KCGrid::C2VI(float x, float y, float d)
{
  try
  {
    return GI2VI(cont2disc(x, xlim, X), cont2disc(y, ylim, Y), cont2disc(d, dlim, D));
  }
  catch(const out_of_range& e)
  {
    stringstream ss;
    ss << e.what() << endl;
    ss << "continuous coordinate: " << x << " " << y << " " << d << endl;
    throw out_of_range(ss.str());
  }
}

//default limits of lms sensor in willow garage
void KCGrid::assignLimits(
      float xmin = 0, float xmax = 20
      ,float ymin = -20, float ymax = 20
      ,float dmin = 0.1, float dmax = 30)
{
  if((xmin-xmax)==0 || (ymin-ymax)==0 || (dmin-dmax)==0) 
    throw runtime_error("bad limits in assignLimits(...) please check ParamIn::readAllLines()");
  xlim = make_pair(xmin, xmax);
  ylim = make_pair(ymin, ymax);
  dlim = make_pair(dmin, dmax);
}

bool KCGrid::assignLimits( const map<string, any>& m)
{
  try
  {
    assignLimits(
      any_cast<float>(m.at("fxmin")), 
      any_cast<float>(m.at("fxmax")),
      any_cast<float>(m.at("fymin")), 
      any_cast<float>(m.at("fymax")),
      any_cast<float>(m.at("fdmin")), 
      any_cast<float>(m.at("fdmax")));
  }
  catch(const out_of_range& e)
  {
    assignLimits();
    return 1;
  }
  return 0;
}

inline size_t KCGrid::cont2disc(float cont, const pair<float, float>& lim, size_t res)
{
  //lim.first is the minimum limitation 
  //so return the larger of cont or lim.first
  float c, range, resolution;
  c = max(cont, lim.first);
  //lim.second is the maximum limitation
  c = min(c, lim.second);
  //shift and discretize
  c = c - lim.first;
  range = lim.second - lim.first;
  resolution = numeric_cast<float>(res);
  c = c/range * resolution;
  c = floor(c);
  if(c == resolution)
    c = resolution - 1;
  //float -> size_t
  return numeric_cast<size_t>(c);
}

inline float KCGrid::disc2cont(size_t grid_index, const pair<float, float>& lim, size_t res)
{
  assert(grid_index < res && grid_index >= 0);
  return numeric_cast<float>(grid_index)*
         (lim.second - lim.first)/numeric_cast<float>(res) + 
         lim.first;
}

void KCGrid::convert(map<string, any>& m)
{
  string datafilename;
  try
  {
    datafilename = 
      any_cast<string>(
        m.at("databinaryfile"));
  }
  catch(const std::exception& e)
  {
    stringstream ss;
    ss << "KCGrid::convert(map<string, any>) cannot find parameter named databinaryfile." << endl;
    ss << "map size is " << m.size() << endl; 
    ss << "parameters are" << endl;
   for(auto i = m.begin() ; i != m.end() ; ++i)
    {
      if(isString(i->second))
        ss << i->first << " " << any_cast<string>(i->second) << endl;
      if(isDouble(i->second) || isFloat(i->second))
        ss << i->first << " " << any_cast<float>(i->second) << endl;
    }
    throw runtime_error(ss.str());
  }

  scoped_ptr<DataIn> datain_ptr_;
  datain_ptr_.reset(new DataIn(datafilename));
  pf_vector_t p;
  laser_feature_t f;
  tf::Quaternion q;
  tf::Vector3 v;
  size_t idx;
  data_count_ = 0;
  while(datain_ptr_->readALine(p, f))
  {
    ++data_count_;
    //create a se3 kernel based on the pose
    nuklei::kernel::se3 k;
    k.loc_.X() = p.v[0];
    k.loc_.Y() = p.v[1];
    q = tf::createQuaternionFromYaw(p.v[2]);
    v = q.getAxis();
    k.ori_.W() = q.getW();
    k.ori_.X() = v.x();
    k.ori_.Y() = v.y();
    k.ori_.Z() = v.z();
    k.setWeight(1);
    // add the kernel to the tree corresponding to  features
    idx = C2VI(f.x, f.y, f.dist);
    if(tree_map_.find(idx) == tree_map_.end())
      tree_map_.insert(
        make_pair(
          idx, TreeMap::mapped_type(new nuklei::KernelCollection)));
    tree_map_.at(idx)->add(k);
  }
  if(data_count_ == 0)
  {
    datain_ptr_->test();
    string err_msg("KCGrid::convert(map<string, any>) cannot read binary data named \"");
    err_msg += datafilename;
    err_msg += '\"';
    throw ios_base::failure(err_msg);
  }

  //TODO remember to release the allocated memory
  //TODO float* -> unsigned int*
  data_matrix_.reset( new float[3*tree_map_.size()]);
  float* temp_ptr = data_matrix_.get();
  vector<size_t> temp_vec;
  TreeMap::iterator it = tree_map_.begin();
  TreeMap::iterator end = tree_map_.end();
  idx = 1;
  for(; it != end ; ++it, ++idx)
  {
    //set kernel bandwidth
    //normaliize kdts

    it->second->setKernelLocH(10);
    it->second->setKernelOriH(.4);
    it->second->normalizeWeights();
    it->second->totalWeight();
    it->second->buildKdTree();
    it->second->totalWeight();
    //draw a sample from the kdtree
    int i = 0;
    double ax, ay, az;
/*
    for(nuklei::KernelCollection::const_sample_iterator iter = as_const(*it->second).sampleBegin(1);
         iter != iter.end(); ++iter, ++i)
    {
      // *iter returns a reference to a datapoint/kernel of tree
      // iter.index() returns the index (in tree) of that element.
      std::auto_ptr<nuklei::kernel::se3> se3_pose = (*iter).polySe3Sample();
      cout << "sample se3 pose: " << *se3_pose << endl;
    }
*/
    //for building a kdtree for nn search
    gridcell_indices_.push_back(it->first);
    VI2GI(it->first, temp_vec);
    temp_ptr[0] = disc2cont(temp_vec[0], xlim, X);
    temp_ptr[1] = disc2cont(temp_vec[1], ylim, Y);
    temp_ptr[2] = disc2cont(temp_vec[2], dlim, D);
    temp_ptr += 3;
  }
  temp_ptr = NULL;

  //TODO build a kdtree for 1 nearest neighbor search
  //when a new coming laser scan has identical features
  //there is a possibility that no corresponding density tree exists
  //therefore, using kdtree can provide the most closest density tree according to the feature values
  flann_index_.reset(
    new FLANNIndex(
      ::flann::Matrix<float>(
        data_matrix_.get(),
        tree_map_.size(),
        3//dimension is 3 x, y, and d
      ),
      ::flann::KDTreeSingleIndexParams(2)//this is a discrete kdtree so doesn't need very much nodes
    )
  );
  flann_index_->buildIndex();
}

size_t KCGrid::nnSearch(float x, float y, float d)
{
  vector<float> query_point_(3);
  query_point_[0] = x;
  query_point_[1] = y;
  query_point_[2] = d;
  int k_ = 1;
  vector<int> k_indices_(k_);
  vector<float> k_distances_(k_);
  ::flann::Matrix<float> query_mat (&query_point_[0], 1, 3);
  ::flann::Matrix<int> k_indices_mat (&k_indices_[0], 1, k_);
  ::flann::Matrix<float> k_distances_mat (&k_distances_[0], 1, k_);
  ::flann::SearchParams params_;
  flann_index_->knnSearch(
    query_mat, 
    k_indices_mat, 
    k_distances_mat, 
    k_, params_);
  cout << "query point: ";
  for(int i = 0 ; i < query_point_.size() ; ++i)
    cout << query_point_[i] << " ";
  cout << endl << "indices: ";
  for(int i = 0 ; i < k_indices_.size() ; ++i)
    cout << k_indices_[i] << " ";
  cout << endl << "distances: ";
  for(int i = 0 ; i < k_distances_.size() ; ++i)
    cout << k_distances_[i] << " ";
  cout << endl;
  float* ptr = data_matrix_.get() + 3*k_indices_[0];
  float fx = ptr[0];
  float fy = ptr[1];
  float fd = ptr[2];
  cout << "nearest pose:" << ptr[0] << ' ' << ptr[1] << ' ' << ptr[2] << endl;
  assert(gridcell_indices_[k_indices_[0]] == GI2VI(ptr[0], ptr[1], ptr[2]));
  return gridcell_indices_[k_indices_[0]];
}



