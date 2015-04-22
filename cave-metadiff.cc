#include <paludis/spec_tree.hh>
#include <paludis/paludis.hh>
#include "example_command_line.hh"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <utility>
#include <algorithm>

using namespace paludis;
using namespace examples;

typedef struct {
    std::shared_ptr<const Choice>      c;
    std::shared_ptr<const ChoiceValue> val;
} cvmap_entry;
typedef std::map<std::string,cvmap_entry> cvmap;

void compare_choices(const std::shared_ptr<const Choices> &c1,const std::shared_ptr<const Choices> &c2) {
    cvmap cv1,cv2;
    for( Choices::ConstIterator choice_it = c1->begin() ; choice_it != c1->end() ; ++choice_it ) {
        std::shared_ptr<const Choice> c = *choice_it;
        if ( ! c->consider_added_or_changed() || c->contains_every_value() )
            continue;
        for(Choice::ConstIterator val_it = c->begin(); val_it != c->end(); ++val_it ) {
            std::shared_ptr<const ChoiceValue> val = *val_it;
            cv1[val->name_with_prefix().value()] = { .c = c, .val = val };
        }
    }
    
    for( Choices::ConstIterator choice_it = c2->begin() ; choice_it != c2->end() ; ++choice_it ) {
        std::shared_ptr<const Choice> c = *choice_it;
        if ( ! c->consider_added_or_changed() || c->contains_every_value() )
            continue;
        for(Choice::ConstIterator val_it = c->begin(); val_it != c->end(); ++val_it ) {
            std::shared_ptr<const ChoiceValue> val = *val_it;
            cv2[val->name_with_prefix().value()] = { .c = c, .val = val };
        }
    }
    
    // find differences between both maps
    for(cvmap::iterator it = cv1.begin(); it != cv1.end(); ++it) {
        // check if the key is in the second map
        cvmap::iterator c_it = cv2.find(it->first);
        if ( c_it == cv2.end() ) {
            std::cout << it->second.c->human_name() << " " << it->second.val->unprefixed_name().value() << " (" << it->second.val->origin() << ") does not exist in new package" << std::endl;
            continue;
        }
        
        // compare the values
        if ( it->second.val->enabled() != c_it->second.val->enabled() ) {
            std::cout << it->second.c->human_name() << " " << it->second.val->unprefixed_name().value() << " has different states between packages" << std::endl;
        }
        // remove the key from the second map, as we are done with it
        cv2.erase(c_it);
    }
    
    // check if we have keys only for the new package
    for(cvmap::iterator it = cv2.begin(); it != cv2.end(); ++it)
        std::cout << it->second.c->human_name() << " " << it->second.val->unprefixed_name().value() << " (" << it->second.val->origin() << ") does not exist in installed package" << std::endl;
}

void print_choices(const std::shared_ptr<const Choices> &choices) {
    for( Choices::ConstIterator choice_it = choices->begin() ; choice_it != choices->end() ; ++choice_it ) {
        std::shared_ptr<const Choice> c = *choice_it;
        if ( ! c->consider_added_or_changed() )
            continue;
        std::cout << "C: " << c->human_name() << std::endl;
        for(Choice::ConstIterator val_it = c->begin(); val_it != c->end(); ++val_it ) {
            std::shared_ptr<const ChoiceValue> val = *val_it;
            std::cout << " " << ( val->enabled() ? "+" : "-" ) << val->unprefixed_name() << std::endl;
        }
    }
}

int main(int argc, char **argv)
{

    CommandLine::get_instance()->run(argc, argv,"cave-metadiff", "CAVE_METADIFF_OPTIONS", "CAVE_METADIFF_CMDLINE");
    std::shared_ptr<Environment> env(EnvironmentFactory::get_instance()->create(CommandLine::get_instance()->a_environment.argument()));
    for (CommandLine::ParametersConstIterator q(CommandLine::get_instance()->begin_parameters()),
            q_end(CommandLine::get_instance()->end_parameters()) ; q != q_end ; ++q)
    {
        std::cout << "looking for package " << *q << std::endl;
        PackageDepSpec spec(parse_user_package_dep_spec(*q, env.get(), { updso_allow_wildcards, updso_throw_if_set }));
        
        Selection installed = selection::BestVersionOnly(
            generator::Matches(spec,nullptr,{ } )
            | filter::InstalledAtSlash()
        );
        std::shared_ptr<const PackageIDSequence> installed_ids((*env)[installed]);
        
        if ( installed_ids->empty() ) {
            std::cout << "package " << spec.text() << " not installed" << std::endl;
            continue;
        }
        
        std::shared_ptr<const PackageID> installed_id = *(installed_ids->begin());
        
        Selection would_install = selection::BestVersionOnly(
            generator::Matches(spec,nullptr,{ } )
            | filter::SupportsAction<InstallAction>()
            | filter::NotMasked()
        );
        std::shared_ptr<const PackageIDSequence> would_install_ids((*env)[would_install]);
        
        if ( would_install_ids->empty() ) {
            std::cout << "package " << spec.text() << " has no installable version" << std::endl;
        }
        
        std::shared_ptr<const PackageID> would_install_id = *(would_install_ids->begin());
        
        if ( installed_id->version() != would_install_id->version() ) {
            std::cout << "* new version differs from installed version";
            continue;
        }
        
        compare_choices(installed_id->choices_key()->parse_value(),would_install_id->choices_key()->parse_value());
        //print_choices(installed_id->choices_key()->parse_value());
        //print_choices(would_install_id->choices_key()->parse_value());
        
        //std::shared_ptr<const DependencySpecTree> installed_deps = installed_id->dependencies_key()->parse_value();
        //auto test = installed_deps->top() ;
    }
}
