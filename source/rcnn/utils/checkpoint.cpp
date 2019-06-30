#include "checkpoint.h"
#include <fstream>
#include <iostream>
#include <torch/script.h>
#include <torch/serialize/archive.h>
#include <cassert>


namespace rcnn{
namespace utils{

Checkpoint::Checkpoint(rcnn::modeling::GeneralizedRCNN& model, 
                    rcnn::solver::ConcatOptimizer& optimizer, 
                    rcnn::solver::ConcatScheduler& scheduler, 
                    std::string save_dir)
                    :model_(model),
                     optimizer_(optimizer),
                     scheduler_(scheduler),
                     save_dir_(save_dir){}

int Checkpoint::load(std::string weight_path){
  //return iteration
  torch::NoGradGuard guard;
  torch::Tensor iter = torch::zeros({1});
  bool checker = true;
  if(has_checkpoint()){
    return load_from_checkpoint();
  }
  else{
    //no optimizer scheduler
    auto module = torch::jit::load(weight_path);
    auto buffer = module->find_buffer("iteration");

    // archive.load_from(weight_path);
    if(buffer != nullptr)
      return load_from_checkpoint();
    else{
      std::vector<std::string> names_in_pth;
      for(auto& i : module->get_parameters()){
        names_in_pth.push_back(i.name());
      }

      for(auto& i : model_->named_parameters()){
        for(auto& name : names_in_pth){
          if(i.key().find(name) != std::string::npos){
            auto param = module->find_parameter(name);
            assert(param != nullptr);
            i.value().copy_(param->value().toTensor());
            checker = false;
            std::cout << i.key() << " loaded from " << name << "\n";
            // archive.read(name, i.value());
          }
        }
        // if(i.key().find("backbone") != std::string::npos){
        //   archive.try_read(i.key().substr(20), i.value());
        //   // std::cout << a << " " << i.key().substr(20) << "\n";
        // }
        // else
        //   archive.try_read(i.key(), i.value());
      }
      for(auto& i : model_->named_buffers()){
        for(auto& name : names_in_pth){
          if(i.key().find(name) != std::string::npos){
            auto param = module->find_buffer(name);
            assert(param != nullptr);
            i.value().copy_(param->value().toTensor());
            checker = false;
            std::cout << i.key() << " loaded from " << name << "\n";
          }
        }
        // if(i.key().find("backbone") != std::string::npos)
        //   archive.try_read(i.key().substr(20), i.value(), true);
        // else
        //   archive.try_read(i.key(), i.value(), true);
      }
      if(checker) std::cout << "No checkpoint found. Initializing model from scratch\n";
      return 0;
    }
  }
}

int Checkpoint::load_from_checkpoint(){
  torch::Tensor iter = torch::zeros({1}).to(torch::kI64);
  std::string checkpoint_name = get_checkpoint_file();
  torch::serialize::InputArchive archive;
  archive.load_from(checkpoint_name);
  model_->load(archive);
  optimizer_.load(archive);
  scheduler_.load(archive);
  
  archive.read("iteration", iter, true);
  return iter.item<int>();
}

void Checkpoint::save(std::string name, int iteration){
  torch::serialize::OutputArchive archive;
  auto iter = torch::tensor({iteration}).to(torch::kI64);
  archive.write("iteration", iter, true);
  std::cout << "Saving checkpoint to " << save_dir_ + "/" + name << "\n";
  model_->save(archive);
  optimizer_.save(archive);
  scheduler_.save(archive);
  archive.save_to(save_dir_ + "/" + name);
  write_checkpoint_file(name);
}

bool Checkpoint::has_checkpoint(){
  std::ifstream f(save_dir_ + "/last_checkpoint");
  return f.good();
}

std::string Checkpoint::get_checkpoint_file(){
  std::ifstream inFile(save_dir_ + "/last_checkpoint");
  std::string checkpoint_name;
  std::getline(inFile, checkpoint_name);
  return checkpoint_name;
}

void Checkpoint::write_checkpoint_file(std::string name){
  std::ofstream writeFile(save_dir_ + "/last_checkpoint");
  writeFile << save_dir_ + "/" + name;
  writeFile.close();
}


}
}