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

struct ComparingPrettyPrinter :
        UnformattedPrettyPrinter
{
    using UnformattedPrettyPrinter::prettify;

    const std::string prettify(const PackageDepSpec & s) const
    {
        /* cat/pkg[foo][bar] and cat/pkg[bar][foo] are the same, and := deps
         * are weird */
        std::set<std::string> tokens;

        if (s.package_ptr())
            tokens.insert("package:" + stringify(*s.package_ptr()));

        if (s.package_name_part_ptr())
            tokens.insert("package_name_part:" + stringify(*s.package_name_part_ptr()));

        if (s.category_name_part_ptr())
            tokens.insert("category_name_part:" + stringify(*s.category_name_part_ptr()));

        if (s.version_requirements_ptr())
        {
            for (VersionRequirements::ConstIterator r(s.version_requirements_ptr()->begin()),
                    r_end(s.version_requirements_ptr()->end()) ; r != r_end ; ++r)
                tokens.insert("version_requirement:" + stringify(s.version_requirements_mode()) +
                              stringify(r->version_operator()) + stringify(r->version_spec()));
        }

        if (s.slot_requirement_ptr())
        {
            /* since the EAPI5 syntax for rewritten := deps in the VDB
             * doesn't allow us to tell whether the dep was originally a :=
             * kind or a :slot= kind, normalise them all to the same thing
             * to avoid spurious differences */
            auto r(s.slot_requirement_ptr()->maybe_original_requirement_if_rewritten());
            tokens.insert("slot_requirement:" + (r ? r : s.slot_requirement_ptr())->make_accept_returning(
            [&] (const SlotAnyAtAllLockedRequirement &)   {
                return std::string { ":=" };
            },
            [&] (const SlotAnyPartialLockedRequirement &) {
                return std::string { ":=" };
            },
            [&] (const SlotUnknownRewrittenRequirement &) {
                return std::string { ":=" };
            },
            [&] (const SlotRequirement & q)               {
                return stringify(q);
            }
                          ));
        }

        if (s.in_repository_ptr())
            tokens.insert("in_repository:" + stringify(*s.in_repository_ptr()));

        if (s.installable_to_repository_ptr())
            tokens.insert("installable_to_repository:" + stringify(s.installable_to_repository_ptr()->repository())
                          + "/" + stringify(s.installable_to_repository_ptr()->include_masked()));

        if (s.from_repository_ptr())
            tokens.insert("from_repository:" + stringify(*s.from_repository_ptr()));

        if (s.installed_at_path_ptr())
            tokens.insert("installed_at_path:" + stringify(*s.installed_at_path_ptr()));

        if (s.installable_to_path_ptr())
            tokens.insert("installable_to_path:" + stringify(s.installable_to_path_ptr()->path())
                          + "/" + stringify(s.installable_to_path_ptr()->include_masked()));

        if (s.additional_requirements_ptr())
        {
            for (AdditionalPackageDepSpecRequirements::ConstIterator u(s.additional_requirements_ptr()->begin()),
                    u_end(s.additional_requirements_ptr()->end()) ; u != u_end ; ++u)
                tokens.insert("additional_requirement:" + stringify(**u));
        }

        return "PackageDepSpec(" + join(tokens.begin(), tokens.end(), ";") + ")";
    }
};

bool is_same_dependencies(
    const std::shared_ptr<const MetadataSpecTreeKey<DependencySpecTree> > & a,
    const std::shared_ptr<const MetadataSpecTreeKey<DependencySpecTree> > & b)
{
    if (! a)
        return ! b;
    else if (! b)
        return false;

    auto sa(a->pretty_print_value(ComparingPrettyPrinter(), { }));
    auto sb(b->pretty_print_value(ComparingPrettyPrinter(), { }));

    if (sa != sb) {
        std::cout << "  [ ( " << a->human_name() << " )" << std::endl;
        std::cout << "    -" << sa << std::endl;
        std::cout << "    +" << sb << std::endl;
        std::cout << "  ]" << std::endl;
    }
    return sa == sb;
}

void
get_sameness(
    const std::shared_ptr<const PackageID> & existing_id,
    const std::shared_ptr<const PackageID> & installable_id)
{
    bool is_same_version(existing_id->version() == installable_id->version());
    bool is_same(false);
    bool is_same_metadata(false);

    if (is_same_version)
    {
        is_same = true;
        is_same_metadata = true;

        std::set<ChoiceNameWithPrefix> common;
        std::shared_ptr<const Choices> installable_choices;
        std::shared_ptr<const Choices> existing_choices;

        if (existing_id->choices_key() && installable_id->choices_key())
        {
            installable_choices = installable_id->choices_key()->parse_value();
            existing_choices = existing_id->choices_key()->parse_value();

            std::set<ChoiceNameWithPrefix> i_common, u_common;
            for (Choices::ConstIterator k(installable_choices->begin()), k_end(installable_choices->end()) ;
                    k != k_end ; ++k)
            {
                if (! (*k)->consider_added_or_changed())
                    continue;

                for (Choice::ConstIterator i((*k)->begin()), i_end((*k)->end()) ;
                        i != i_end ; ++i)
                    if (co_explicit == (*i)->origin())
                        i_common.insert((*i)->name_with_prefix());
            }

            for (Choices::ConstIterator k(existing_choices->begin()), k_end(existing_choices->end()) ;
                    k != k_end ; ++k)
            {
                if (! (*k)->consider_added_or_changed())
                    continue;

                for (Choice::ConstIterator i((*k)->begin()), i_end((*k)->end()) ;
                        i != i_end ; ++i)
                    if (co_explicit == (*i)->origin())
                        u_common.insert((*i)->name_with_prefix());
            }

            std::set_intersection(
                i_common.begin(), i_common.end(),
                u_common.begin(), u_common.end(),
                std::inserter(common, common.begin()));
        }

        for (std::set<ChoiceNameWithPrefix>::const_iterator f(common.begin()), f_end(common.end()) ;
                f != f_end ; ++f)
            if (installable_choices->find_by_name_with_prefix(*f)->enabled() !=
                    existing_choices->find_by_name_with_prefix(*f)->enabled())
            {
                std::cout << "* " << (*f).value() << " differs" << std::endl;
            }
        is_same_dependencies(existing_id->build_dependencies_key(), installable_id->build_dependencies_key());
        is_same_dependencies(existing_id->run_dependencies_key(), installable_id->run_dependencies_key());
        is_same_dependencies(existing_id->post_dependencies_key(), installable_id->post_dependencies_key());
        is_same_dependencies(existing_id->dependencies_key(), installable_id->dependencies_key());
    } else {
        std::cout << "* new version differs from installed version" << std::endl;
    }
}


int main(int argc, char **argv)
{

    CommandLine::get_instance()->run(argc, argv,"cave-metadiff", "CAVE_METADIFF_OPTIONS", "CAVE_METADIFF_CMDLINE");
    std::shared_ptr<Environment> env(EnvironmentFactory::get_instance()->create(CommandLine::get_instance()->a_environment.argument()));
    for (CommandLine::ParametersConstIterator q(CommandLine::get_instance()->begin_parameters()),
            q_end(CommandLine::get_instance()->end_parameters()) ; q != q_end ; ++q)
    {
        PackageDepSpec spec(parse_user_package_dep_spec(*q, env.get(), { updso_throw_if_set }));
        std::cout << spec.text() << ":" << std::endl;

        Selection installed = selection::BestVersionOnly(
                                  generator::Matches(spec,nullptr, { } )
                                  | filter::InstalledAtSlash()
                              );
        std::shared_ptr<const PackageIDSequence> installed_ids((*env)[installed]);

        if ( installed_ids->empty() ) {
            std::cout << "package " << spec.text() << " not installed" << std::endl;
            continue;
        }

        std::shared_ptr<const PackageID> installed_id = *(installed_ids->begin());

        Selection would_install = selection::BestVersionOnly(
                                      generator::Matches(spec,nullptr, { } )
                                      | filter::SupportsAction<InstallAction>()
                                      | filter::NotMasked()
                                  );
        std::shared_ptr<const PackageIDSequence> would_install_ids((*env)[would_install]);

        if ( would_install_ids->empty() ) {
            std::cout << "package " << spec.text() << " has no installable version" << std::endl;
        }

        std::shared_ptr<const PackageID> would_install_id = *(would_install_ids->begin());

        get_sameness(installed_id,would_install_id);
        std::cout << std::endl;

    }
}
