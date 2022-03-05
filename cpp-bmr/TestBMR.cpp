#include <bmr.hpp>

#include <gflags/gflags.h>

DEFINE_string(op, "", "backup/restore/blockcopy/imagerecovery");
DEFINE_string(level, "", "full/incr");
DEFINE_string(bcd, "", "Backup Component Document");
DEFINE_string(src, "", "Source");
DEFINE_string(dest, "", "Destination");

int main(int argc, char *argv[])
{
  bool fRet = false;

  gflags::SetVersionString("1.0.0");
  gflags::SetUsageMessage("BMR Backup and Recovery");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_op == "backup")
  {
    fRet = BMR::Backup(FLAGS_level, FLAGS_dest);
  }
  else if (FLAGS_op == "restore")
  {
    fRet = BMR::Restore(FLAGS_bcd, FLAGS_dest);
  }
  else if (FLAGS_op == "blockcopy")
  {
    fRet = BMR::BlockCopy(FLAGS_src, FLAGS_dest);
  }
  else if (FLAGS_op == "imagerecovery")
  {
    fRet = BMR::ImageRecover(FLAGS_src, FLAGS_dest);
  }

  return fRet;
}